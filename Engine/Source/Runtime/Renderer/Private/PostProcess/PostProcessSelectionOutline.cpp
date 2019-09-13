// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PostProcess/PostProcessSelectionOutline.h"
#include "SceneTextureParameters.h"
#include "CanvasTypes.h"
#include "RenderTargetTemp.h"
#include "ClearQuad.h"

namespace
{
const uint32 MsaaSampleCountMax = 8;

class FSelectionOutlinePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSelectionOutlinePS);
	SHADER_USE_PARAMETER_STRUCT(FSelectionOutlinePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Color)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Depth)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportTransform, ColorToDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
		SHADER_PARAMETER_RDG_TEXTURE(, EditorPrimitivesDepth)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(, EditorPrimitivesStencil)
		SHADER_PARAMETER(FVector, OutlineColor)
		SHADER_PARAMETER(float, SelectionHighlightIntensity)
		SHADER_PARAMETER(FVector, SubduedOutlineColor)
		SHADER_PARAMETER(float, BSPSelectionIntensity)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FSampleCountDimension : SHADER_PERMUTATION_INT("MSAA_SAMPLE_COUNT", MsaaSampleCountMax);
	using FPermutationDomain = TShaderPermutationDomain<FSampleCountDimension>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		const int32 SampleCount = PermutationVector.Get<FSampleCountDimension>();

		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
		{
			if(SampleCount > 1)
			{
				return false;
			}
		}

		// Only compile valid sample counts.
		switch (SampleCount)
		{
		case 1:
		case 2:
		case 4:
		case 8:
			return IsPCPlatform(Parameters.Platform);
		}

		return false;
	}
};

IMPLEMENT_GLOBAL_SHADER(FSelectionOutlinePS, "/Engine/Private/PostProcessSelectionOutline.usf", "MainPS", SF_Pixel);
} //! namespace

FRDGTextureRef AddSelectionOutlinePass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureRef SceneColorTexture,
	FIntRect SceneColorViewRect,
	FRDGTextureRef SceneDepthTexture,
	FIntRect SceneDepthViewRect)
{
	check(SceneColorTexture);
	check(!SceneColorViewRect.IsEmpty());
	check(SceneDepthTexture);
	check(!SceneDepthViewRect.IsEmpty());

	RDG_EVENT_SCOPE(GraphBuilder, "EditorSelectionOutlines");

	const FViewInfo& View = ScreenPassView.View;

	uint32 MsaaSampleCount = 0;

	// Patch uniform buffers with updated state for rendering the outline mesh draw commands.
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);

		MsaaSampleCount = SceneContext.GetEditorMSAACompositingSampleCount();

		FViewInfo& EditorView = *View.CreateSnapshot();

		// Patch view rect.
		EditorView.ViewRect = SceneColorViewRect;

		// Override pre-exposure to 1.0f, because rendering after tonemapper.
		EditorView.PreExposure = 1.0f;

		// Kills material texture mip-bias because after TAA.
		EditorView.MaterialTextureMipBias = 0.0f;

		if (EditorView.AntiAliasingMethod == AAM_TemporalAA)
		{
			EditorView.ViewMatrices.HackRemoveTemporalAAProjectionJitter();
		}

		FScene* Scene = View.Family->Scene->GetRenderScene();

		FSceneTexturesUniformParameters SceneTextureParameters;
		SetupSceneTextureUniformParameters(SceneContext, View.FeatureLevel, ESceneTextureSetupMode::None, SceneTextureParameters);
		Scene->UniformBuffers.EditorSelectionPassUniformBuffer.UpdateUniformBufferImmediate(SceneTextureParameters);

		FBox VolumeBounds[TVC_MAX];
		FViewUniformShaderParameters ViewUniformShaderParameters;
		EditorView.SetupUniformBufferParameters(SceneContext, VolumeBounds, TVC_MAX, ViewUniformShaderParameters);
		Scene->UniformBuffers.UpdateViewUniformBufferImmediate(ViewUniformShaderParameters);
	}

	FRDGTextureRef DepthStencilTexture = nullptr;

	// Generate custom depth / stencil for outline shapes.
	{
		{
			FRDGTextureDesc DepthStencilDesc = SceneColorTexture->Desc;
			DepthStencilDesc.Reset();
			DepthStencilDesc.Format = PF_DepthStencil;
			DepthStencilDesc.Flags = TexCreate_None;

			// This is a reversed Z depth surface, so 0.0f is the far plane.
			DepthStencilDesc.ClearValue = FClearValueBinding((float)ERHIZBuffer::FarPlane, 0);

			// Mark targetable as TexCreate_ShaderResource because we actually do want to sample from the unresolved MSAA target in this case.
			DepthStencilDesc.TargetableFlags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
			DepthStencilDesc.NumSamples = MsaaSampleCount;
			DepthStencilDesc.bForceSharedTargetAndShaderResource = true;

			DepthStencilTexture = GraphBuilder.CreateTexture(DepthStencilDesc, TEXT("SelectionOutline"));
		}

		FRenderTargetParameters* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthStencilTexture,
			ERenderTargetLoadAction::EClear,
			ERenderTargetLoadAction::EClear,
			FExclusiveDepthStencil::DepthWrite_StencilWrite);

		const FIntPoint SceneColorTextureExtent = SceneColorTexture->Desc.Extent;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("OutlineDepth %dx%d", SceneColorViewRect.Width(), SceneColorViewRect.Height()),
			PassParameters,
			ERDGPassFlags::Raster,
			[&View, SceneColorViewRect, SceneColorTextureExtent](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(SceneColorViewRect.Min.X, SceneColorViewRect.Min.Y, 0.0f, SceneColorViewRect.Max.X, SceneColorViewRect.Max.Y, 1.0f);

			// Run selection pass on static elements
			View.ParallelMeshDrawCommandPasses[EMeshPass::EditorSelection].DispatchDraw(nullptr, RHICmdList);

			// to get an outline around the objects if it's partly outside of the screen
			{
				FIntRect InnerRect = SceneColorViewRect;

				// 1 as we have an outline that is that thick
				InnerRect.InflateRect(-1);

				// top
				RHICmdList.SetScissorRect(true, SceneColorViewRect.Min.X, SceneColorViewRect.Min.Y, SceneColorViewRect.Max.X, InnerRect.Min.Y);
				DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, SceneColorTextureExtent, FIntRect());
				// bottom
				RHICmdList.SetScissorRect(true, SceneColorViewRect.Min.X, InnerRect.Max.Y, SceneColorViewRect.Max.X, SceneColorViewRect.Max.Y);
				DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, SceneColorTextureExtent, FIntRect());
				// left
				RHICmdList.SetScissorRect(true, SceneColorViewRect.Min.X, SceneColorViewRect.Min.Y, InnerRect.Min.X, SceneColorViewRect.Max.Y);
				DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, SceneColorTextureExtent, FIntRect());
				// right
				RHICmdList.SetScissorRect(true, InnerRect.Max.X, SceneColorViewRect.Min.Y, SceneColorViewRect.Max.X, SceneColorViewRect.Max.Y);
				DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, SceneColorTextureExtent, FIntRect());

				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
			}
		});
	}

	FRDGTextureRef OutputTexture = nullptr;

	// Render selection outlines.
	{
		{
			FRDGTextureDesc OutputDesc = SceneColorTexture->Desc;
			OutputDesc.Reset();
			OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("SelectionOutlineColor"));
		}

		const FScreenPassTextureViewport ColorViewport(SceneColorViewRect, SceneColorTexture);
		const FScreenPassTextureViewport DepthViewport(SceneDepthViewRect, SceneDepthTexture);

		FRHISamplerState* PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FSelectionOutlinePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSelectionOutlinePS::FParameters>();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ScreenPassView.GetOverwriteLoadAction());
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Color = GetScreenPassTextureViewportParameters(ColorViewport);
		PassParameters->Depth = GetScreenPassTextureViewportParameters(DepthViewport);
		PassParameters->ColorToDepth = GetScreenPassTextureViewportTransform(PassParameters->Color, PassParameters->Depth);
		PassParameters->ColorTexture = SceneColorTexture;
		PassParameters->ColorSampler = PointClampSampler;
		PassParameters->DepthTexture = SceneDepthTexture;
		PassParameters->DepthSampler = PointClampSampler;
		PassParameters->EditorPrimitivesDepth = DepthStencilTexture;
		PassParameters->EditorPrimitivesStencil = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateWithPixelFormat(DepthStencilTexture, PF_X24_G8));
		PassParameters->OutlineColor = FVector(View.SelectionOutlineColor);
		PassParameters->SelectionHighlightIntensity = GEngine->SelectionHighlightIntensity;
		PassParameters->SubduedOutlineColor = FVector(View.SubduedSelectionOutlineColor);
		PassParameters->BSPSelectionIntensity = GEngine->BSPSelectionHighlightIntensity;

		FSelectionOutlinePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSelectionOutlinePS::FSampleCountDimension>(MsaaSampleCount);

		TShaderMapRef<FSelectionOutlinePS> PixelShader(View.ShaderMap, PermutationVector);

		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("OutlineColor %dx%d", SceneColorViewRect.Width(), SceneColorViewRect.Height()),
			ScreenPassView,
			ColorViewport,
			ColorViewport,
			*PixelShader,
			PassParameters);
	}

	return OutputTexture;
}

FRenderingCompositeOutputRef AddSelectionOutlinePass(FRenderingCompositionGraph& Graph, FRenderingCompositeOutputRef Input)
{
	FRenderingCompositePass* Pass = Graph.RegisterPass(
		new(FMemStack::Get()) TRCPassForRDG<1, 1>(
			[](FRenderingCompositePass* InPass, FRenderingCompositePassContext& InContext)
	{
		FRDGBuilder GraphBuilder(InContext.RHICmdList);
		const FScreenPassViewInfo ScreenPassView(InContext.View);

		FRDGTextureRef SceneColorTexture = InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input0, TEXT("SceneColor"));
		const FIntRect SceneColorViewRect = InContext.SceneColorViewRect;

		const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);
		FRDGTextureRef SceneDepthTexture = GraphBuilder.RegisterExternalTexture(SceneContext.SceneDepthZ, TEXT("SceneDepthZ"));

		FRDGTextureRef OutputTexture = AddSelectionOutlinePass(
			GraphBuilder,
			ScreenPassView,
			SceneColorTexture,
			SceneColorViewRect,
			SceneDepthTexture,
			InContext.View.ViewRect);

		InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, OutputTexture);

		GraphBuilder.Execute();
	}));
	Pass->SetInput(ePId_Input0, Input);
	return Pass;
}

#endif