// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ScreenPass.h"
#include "EngineGlobals.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"
#include "RenderGraphUtils.h"

extern bool IsPostProcessingWithComputeEnabled(const FViewInfo& View);

IMPLEMENT_GLOBAL_SHADER(FScreenPassVS, "/Engine/Private/ScreenPass.usf", "ScreenPassVS", SF_Vertex);

const FTextureRHIRef& GetMiniFontTexture()
{
	if (GEngine->MiniFontTexture)
	{
		return GEngine->MiniFontTexture->Resource->TextureRHI;
	}
	else
	{
		return GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture;
	}
}

bool IsHMDHiddenAreaMaskActive()
{
	// Query if we have a custom HMD post process mesh to use
	static const auto* const HiddenAreaMaskCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.HiddenAreaMask"));

	return
		HiddenAreaMaskCVar != nullptr &&
		HiddenAreaMaskCVar->GetValueOnRenderThread() == 1 &&
		GEngine &&
		GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() &&
		GEngine->XRSystem->GetHMDDevice()->HasVisibleAreaMesh();
}

FScreenPassTextureViewportParameters GetScreenPassTextureViewportParameters(const FScreenPassTextureViewport& InViewport)
{
	const FVector2D Extent(InViewport.Extent);
	const FVector2D ViewportMin(InViewport.Rect.Min.X, InViewport.Rect.Min.Y);
	const FVector2D ViewportMax(InViewport.Rect.Max.X, InViewport.Rect.Max.Y);
	const FVector2D ViewportSize = ViewportMax - ViewportMin;

	FScreenPassTextureViewportParameters Parameters;

	if (!InViewport.IsEmpty())
	{
		Parameters.Extent = Extent;
		Parameters.ExtentInverse = FVector2D(1.0f / Extent.X, 1.0f / Extent.Y);

		Parameters.ScreenPosToViewportScale = FVector2D(0.5f, -0.5f) * ViewportSize;
		Parameters.ScreenPosToViewportBias = (0.5f * ViewportSize) + ViewportMin;

		Parameters.ViewportMin = InViewport.Rect.Min;
		Parameters.ViewportMax = InViewport.Rect.Max;

		Parameters.ViewportSize = ViewportSize;
		Parameters.ViewportSizeInverse = FVector2D(1.0f / Parameters.ViewportSize.X, 1.0f / Parameters.ViewportSize.Y);

		Parameters.UVViewportMin = ViewportMin * Parameters.ExtentInverse;
		Parameters.UVViewportMax = ViewportMax * Parameters.ExtentInverse;

		Parameters.UVViewportSize = Parameters.UVViewportMax - Parameters.UVViewportMin;
		Parameters.UVViewportSizeInverse = FVector2D(1.0f / Parameters.UVViewportSize.X, 1.0f / Parameters.UVViewportSize.Y);

		Parameters.UVViewportBilinearMin = Parameters.UVViewportMin + 0.5f * Parameters.ExtentInverse;
		Parameters.UVViewportBilinearMax = Parameters.UVViewportMax - 0.5f * Parameters.ExtentInverse;
	}

	return Parameters;
}

FScreenPassRenderTarget FScreenPassRenderTarget::CreateFromViewFamily(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	const FRenderTarget* RenderTarget = View.Family->RenderTarget;
	const FTexture2DRHIRef& Texture = RenderTarget->GetRenderTargetTexture();
	ensure(Texture.IsValid());

	FSceneRenderTargetItem Item;
	Item.TargetableTexture = Texture;
	Item.ShaderResourceTexture = Texture;

	FPooledRenderTargetDesc Desc;

	if (Texture)
	{
		Desc.Extent = Texture->GetSizeXY();
		Desc.Format = Texture->GetFormat();
		Desc.NumMips = Texture->GetNumMips();
	}
	else
	{
		Desc.Extent = RenderTarget->GetSizeXY();
		Desc.NumMips = 1;
	}

	Desc.DebugName = TEXT("ViewFamilyTarget");
	Desc.TargetableFlags |= TexCreate_RenderTargetable;

	if (RenderTarget->GetRenderTargetUAV().IsValid())
	{
		Desc.TargetableFlags |= TexCreate_UAV;
	}

	TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
	GRenderTargetPool.CreateUntrackedElement(Desc, PooledRenderTarget, Item);

	return FScreenPassRenderTarget(
		GraphBuilder.RegisterExternalTexture(PooledRenderTarget, TEXT("ViewFamilyTarget")),
		// Raw output mode uses the original view rect. Otherwise the final unscaled rect is used.
		View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::RawOutput ? View.ViewRect : View.UnscaledViewRect,
		// First view clears the view family texture; all remaining views load.
		View.IsFirstInFamily() ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad);
}

FScreenPassViewInfo::FScreenPassViewInfo(const FViewInfo& InView)
	: View(InView)
	, bHMDHiddenAreaMaskActive(IsHMDHiddenAreaMaskActive())
	, bUseComputePasses(IsPostProcessingWithComputeEnabled(InView))
{}

void SetScreenPassPipelineState(FRHICommandList& RHICmdList, const FScreenPassPipelineState& ScreenPassDraw)
{
	FRHIPixelShader* PixelShaderRHI = GETSAFERHISHADER_PIXEL(ScreenPassDraw.PixelShader);
	FRHIVertexShader* VertexShaderRHI = GETSAFERHISHADER_VERTEX(ScreenPassDraw.VertexShader);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = ScreenPassDraw.BlendState;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = ScreenPassDraw.DepthStencilState;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShaderRHI;
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShaderRHI;
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
}

void AddDrawTexturePass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureRef InputTexture,
	FRDGTextureRef OutputTexture,
	FIntPoint InputPosition,
	FIntPoint OutputPosition,
	FIntPoint Size)
{
	const FRDGTextureDesc& InputDesc = InputTexture->Desc;
	const FRDGTextureDesc& OutputDesc = OutputTexture->Desc;

	// Use a hardware copy if formats match.
	if (InputDesc.Format == OutputDesc.Format)
	{
		return AddCopyTexturePass(GraphBuilder, InputTexture, OutputTexture, InputPosition, OutputPosition, Size);
	}

	if (Size == FIntPoint::ZeroValue)
	{
		// Copy entire input texture to output texture.
		Size = InputTexture->Desc.Extent;
	}

	// Don't prime color data if the whole texture is being overwritten.
	const ERenderTargetLoadAction LoadAction = (OutputPosition == FIntPoint::ZeroValue && Size == OutputDesc.Extent)
		? ERenderTargetLoadAction::ENoAction
		: ERenderTargetLoadAction::ELoad;

	const FScreenPassTextureViewport InputViewport(InputDesc.Extent, FIntRect(InputPosition, InputPosition + Size));
	const FScreenPassTextureViewport OutputViewport(OutputDesc.Extent, FIntRect(OutputPosition, OutputPosition + Size));

	TShaderMapRef<FCopyRectPS> PixelShader(ScreenPassView.View.ShaderMap);

	FCopyRectPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyRectPS::FParameters>();
	Parameters->InputTexture = InputTexture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, LoadAction);

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("DrawTexturePass"), ScreenPassView, OutputViewport, InputViewport, *PixelShader, Parameters);
}