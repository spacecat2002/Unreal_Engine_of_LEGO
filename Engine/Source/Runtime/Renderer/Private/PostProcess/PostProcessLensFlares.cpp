// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLensFlares.cpp: Post processing lens fares implementation.
=============================================================================*/

#include "PostProcess/PostProcessLensFlares.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

namespace
{
const int32 GLensFlareQuadsPerInstance = 4;

TAutoConsoleVariable<int32> CVarLensFlareQuality(
	TEXT("r.LensFlareQuality"),
	2,
	TEXT(" 0: off but best for performance\n")
	TEXT(" 1: low quality with good performance\n")
	TEXT(" 2: good quality (default)\n")
	TEXT(" 3: very good quality but bad performance"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// The RDG inputs shared by all lens flare passes.
BEGIN_SHADER_PARAMETER_STRUCT(FLensFlarePassParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FLensFlareShader : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("QUADS_PER_INSTANCE"), GLensFlareQuadsPerInstance);
	}

	FLensFlareShader() = default;
	FLensFlareShader(const CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

class FLensFlareBlurVS : public FLensFlareShader
{
public:
	DECLARE_GLOBAL_SHADER(FLensFlareBlurVS);
	SHADER_USE_PARAMETER_STRUCT(FLensFlareBlurVS, FLensFlareShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLensFlarePassParameters, Pass)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER(FIntPoint, TileCount)
		SHADER_PARAMETER(uint32, TileSize)
		SHADER_PARAMETER(float, KernelSize)
		SHADER_PARAMETER(float, KernelAreaInverse)
		SHADER_PARAMETER(float, Threshold)
		SHADER_PARAMETER(float, GuardBandScaleInverse)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FLensFlareBlurVS, "/Engine/Private/PostProcessLensFlares.usf", "LensFlareBlurVS", SF_Vertex);

class FLensFlareBlurPS : public FLensFlareShader
{
public:
	DECLARE_GLOBAL_SHADER(FLensFlareBlurPS);
	SHADER_USE_PARAMETER_STRUCT(FLensFlareBlurPS, FLensFlareShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, BokehTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BokehSampler)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FLensFlareBlurPS, "/Engine/Private/PostProcessLensFlares.usf", "LensFlareBlurPS", SF_Pixel);

class FLensFlareCompositePS : public FLensFlareShader
{
public:
	DECLARE_GLOBAL_SHADER(FLensFlareCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FLensFlareCompositePS, FLensFlareShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLensFlarePassParameters, Pass)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(FLinearColor, FlareColor)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLensFlareCompositePS, "/Engine/Private/PostProcessLensFlares.usf", "LensFlareCompositePS", SF_Pixel);
} //!namespace

enum class ELensFlareQuality : uint32
{
	Disabled,
	Low,
	High,
	MAX
};

ELensFlareQuality GetLensFlareQuality()
{
	return static_cast<ELensFlareQuality>(FMath::Clamp(
		CVarLensFlareQuality.GetValueOnRenderThread(),
		static_cast<int32>(ELensFlareQuality::Disabled),
		static_cast<int32>(ELensFlareQuality::MAX) - 1));
}

struct FLensFlareInputs
{
	static const uint32 LensFlareCountMax = 8;

	// [Required] The bloom convolution texture. If enabled, this will be composited with lens flares. Otherwise,
	// a transparent black texture is used instead. Either way, the final output texture will use the this texture
	// descriptor and viewport.
	FRDGTextureRef BloomTexture = nullptr;
	FIntRect BloomViewRect;

	// [Required] The scene color input, before bloom, which is used as the source of lens flares.
	// This can be a downsampled input based on the desired quality level.
	FRDGTextureRef FlareTexture = nullptr;
	FIntRect FlareViewRect;

	// [Required] The bokeh shape texture to use to blur the lens flares.
	FRHITexture* BokehShapeTexture = nullptr;

	// The number of lens flares to render.
	uint32 LensFlareCount = LensFlareCountMax;

	// The array of per-flare tint colors. Length must be equal to LensFlareCount.
	TArrayView<const FLinearColor> TintColorsPerFlare;

	// The lens flare tint color to apply to all lens flares.
	FLinearColor TintColor;

	// The size of the bokeh shape in screen percentage.
	float BokehSizePercent = 0.0f;

	// Brightness scale of the lens flares.
	float Intensity = 1.0f;

	// Brightness threshold at which lens flares begin having an effect.
	float Threshold = 1.0f;

	// Whether to composite lens flares with the scene color input. If false, the result is composited on transparent black.
	bool bCompositeWithBloom = true;
};

FRDGTextureRef AddLensFlaresPass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	const FLensFlareInputs& Inputs)
{
	check(Inputs.FlareTexture);
	check(!Inputs.FlareViewRect.IsEmpty());
	check(Inputs.BloomTexture);
	check(!Inputs.BloomViewRect.IsEmpty());
	check(Inputs.BokehShapeTexture);
	check(Inputs.LensFlareCount <= FLensFlareInputs::LensFlareCountMax);
	check(Inputs.TintColorsPerFlare.Num() == Inputs.LensFlareCount);

	if (Inputs.BokehSizePercent <= 0.0f || Inputs.Intensity <= 0.0f || Inputs.LensFlareCount == 0)
	{
		return nullptr;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "LensFlares");

	const float PercentToScale = 0.01f;

	// This constant scales the lens flare blur viewport so that the bokeh shape doesn't clip. However,
	// changing this constant will not preserve energy properly. The kernel size should also change based
	// on this constant. This is not implemented in order to provide preserve the look of content. The masking
	// behavior is also affected by this constant.
	const float GuardBandScale = 2.0f;

	const FScreenPassTextureViewport BloomViewport(Inputs.BloomViewRect, Inputs.BloomTexture);
	const FIntPoint BloomViewportSize(BloomViewport.Rect.Size());
	const FScreenPassTextureViewport FlareViewport(Inputs.FlareViewRect, Inputs.FlareTexture);
	const FIntPoint FlareViewSize = FlareViewport.Rect.Size();

	FRHIBlendState* AdditiveBlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
	FRHISamplerState* BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FRDGTextureRef BlurOutputTexture = nullptr;

	// Initialize the blur output texture.
	{
		FRDGTextureDesc BlurOutputDesc = Inputs.FlareTexture->Desc;
		BlurOutputDesc.Reset();
		BlurOutputDesc.ClearValue = FClearValueBinding(FLinearColor::Transparent);

		BlurOutputTexture = GraphBuilder.CreateTexture(BlurOutputDesc, TEXT("LensFlareBlur"));
	}

	// Lens flare blur pass. Rasterizes a bokeh quad for each pixel on the screen based on the intensity threshold.
	{
		const uint32 TileSizeInPixels = 1;

		const FIntPoint TileCount = FlareViewSize / TileSizeInPixels;

		const float KernelSize = (Inputs.BokehSizePercent * static_cast<float>(FlareViewSize.X)) * PercentToScale;

		FLensFlarePassParameters* PassParameters = GraphBuilder.AllocParameters<FLensFlarePassParameters>();
		PassParameters->InputTexture = Inputs.FlareTexture;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(BlurOutputTexture, ERenderTargetLoadAction::EClear);

		// Setup vertex shader parameters.
		FLensFlareBlurVS::FParameters VertexParameters;
		VertexParameters.Pass = *PassParameters;
		VertexParameters.Input = GetScreenPassTextureViewportParameters(FlareViewport);
		VertexParameters.InputSampler = BilinearClampSampler;
		VertexParameters.TileCount = TileCount;
		VertexParameters.TileSize = TileSizeInPixels;
		VertexParameters.KernelSize = KernelSize;
		VertexParameters.Threshold = Inputs.Threshold;
		VertexParameters.KernelAreaInverse = 1.0f / FMath::Max(1.0f, KernelSize * KernelSize);
		VertexParameters.GuardBandScaleInverse = 1.0f / GuardBandScale;

		// Setup pixel shader parameters.
		FLensFlareBlurPS::FParameters PixelParameters;
		PixelParameters.BokehTexture = Inputs.BokehShapeTexture;
		PixelParameters.BokehSampler = BilinearClampSampler;

		TShaderMapRef<FLensFlareBlurVS> VertexShader(ScreenPassView.View.ShaderMap);
		TShaderMapRef<FLensFlareBlurPS> PixelShader(ScreenPassView.View.ShaderMap);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("LensFlareBlur %dx%d", FlareViewSize.X, FlareViewSize.Y),
			PassParameters,
			ERDGPassFlags::Raster,
			[VertexShader, PixelShader, VertexParameters, PixelParameters, AdditiveBlendState, FlareViewport, TileCount]
		(FRHICommandListImmediate& RHICmdList)
		{
			// Viewport is the same as the input.
			RHICmdList.SetViewport(
				FlareViewport.Rect.Min.X,
				FlareViewport.Rect.Min.Y,
				0.0f,
				FlareViewport.Rect.Max.X,
				FlareViewport.Rect.Max.Y,
				1.0f);

			// Apply additive blending pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = AdditiveBlendState;
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			SetShaderParameters(RHICmdList, *VertexShader, VertexShader->GetVertexShader(), VertexParameters);
			SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), PixelParameters);

			// Emit an instanced quad draw call on the order of the number of pixels on the screen.
			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitive(0, 2, FMath::DivideAndRoundUp(TileCount.X * TileCount.Y, GLensFlareQuadsPerInstance));
		});
	}

	FRDGTextureRef LensFlareTexture = nullptr;

	// Initialize the lens flare output texture.
	{
		FRDGTextureDesc LensFlareTextureDesc = Inputs.BloomTexture->Desc;
		LensFlareTextureDesc.Reset();
		LensFlareTextureDesc.ClearValue = FClearValueBinding(FLinearColor::Transparent);

		LensFlareTexture = GraphBuilder.CreateTexture(LensFlareTextureDesc, TEXT("LensFlareTexture"));
	}

	ERenderTargetLoadAction LensFlareLoadAction = ERenderTargetLoadAction::ELoad;

	if (Inputs.bCompositeWithBloom)
	{
		AddCopyTexturePass(
			GraphBuilder,
			Inputs.BloomTexture,
			LensFlareTexture,
			Inputs.BloomViewRect.Min,
			Inputs.BloomViewRect.Min,
			Inputs.BloomViewRect.Size());
	}
	else
	{
		// Clear to transparent black on the first render pass.
		LensFlareLoadAction = ERenderTargetLoadAction::EClear;
	}

	const FIntRect OutputViewRect = BloomViewport.Rect;

	const FVector2D OutputCenter = FVector2D(OutputViewRect.Min + OutputViewRect.Max) / 2;

	// Scales normalized flare tint alpha to a viewport scale factor.
	const float AlphaScale = static_cast<float>(Inputs.LensFlareCount - 1);
	const float AlphaBias = -AlphaScale * 0.5f;

	// Term to normalize the color based on the scale of the guard band.
	const float GuardBandAreaInverse = 1.0f / (GuardBandScale * GuardBandScale);

	const FLinearColor LensFlareHDRColor = Inputs.TintColor * Inputs.Intensity * GuardBandAreaInverse;

	// Render a scaled quad for each lens flare, additively blended onto the target.
	for (uint32 LensFlareIndex = 0; LensFlareIndex < Inputs.LensFlareCount; ++LensFlareIndex)
	{
		const FLinearColor LensFlareTint = Inputs.TintColorsPerFlare[LensFlareIndex];
		
		FLensFlareCompositePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLensFlareCompositePS::FParameters>();
		PassParameters->Pass.InputTexture = BlurOutputTexture;
		PassParameters->Pass.RenderTargets[0] = FRenderTargetBinding(LensFlareTexture, LensFlareLoadAction);
		PassParameters->InputSampler = BilinearClampSampler;
		PassParameters->FlareColor = LensFlareHDRColor * LensFlareTint;

		// Alpha of the tint color is used to derive a scale of the flare quad.
		const float FinalOutputScale = (LensFlareTint.A * AlphaScale + AlphaBias) * GuardBandScale;

		const FVector2D QuadSize = FVector2D(OutputViewRect.Size()) * FinalOutputScale;

		const FVector2D QuadOffset = OutputCenter - 0.5f * QuadSize;

		TShaderMapRef<FLensFlareCompositePS> PixelShader(ScreenPassView.View.ShaderMap);

		const FScreenPassDrawInfo ScreenPassDraw(*ScreenPassView.ScreenPassVS, *PixelShader, AdditiveBlendState);

		// This pass rasterizes the lens flare quad scaled and centered within the viewport.
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("LensFlare%d", LensFlareIndex),
			PassParameters,
			ERDGPassFlags::Raster,
			[PixelShader, PassParameters, OutputViewRect, FlareViewport, QuadSize, QuadOffset, ScreenPassDraw] (FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(OutputViewRect.Min.X, OutputViewRect.Min.Y, 0.0f, OutputViewRect.Max.X, OutputViewRect.Max.Y, 1.0f);

			SetScreenPassPipelineState(RHICmdList, ScreenPassDraw);

			SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), *PassParameters);

			DrawRectangle(
				RHICmdList,
				QuadOffset.X,
				QuadOffset.Y,
				QuadSize.X,
				QuadSize.Y,
				FlareViewport.Rect.Min.X,
				FlareViewport.Rect.Min.Y,
				FlareViewport.Rect.Width(),
				FlareViewport.Rect.Height(),
				OutputViewRect.Size(),
				FlareViewport.Extent,
				ScreenPassDraw.VertexShader,
				EDRF_Default);
		});

		// All subsequent passes must load.
		LensFlareLoadAction = ERenderTargetLoadAction::ELoad;
	}

	return LensFlareTexture;
}

FRenderingCompositeOutputRef AddLensFlarePass(
	FPostprocessContext& Context,
	FRenderingCompositeOutputRef BloomInput,
	TArrayView<FRenderingCompositeOutputRef> SceneColorDownsampleChain,
	bool bCompositeWithBloom)
{
	// Lens Flares
	const FLinearColor LensFlareHDRColor = Context.View.FinalPostProcessSettings.LensFlareTint * Context.View.FinalPostProcessSettings.LensFlareIntensity;

	const ELensFlareQuality LensFlareQuality = GetLensFlareQuality();

	if (LensFlareHDRColor.IsAlmostBlack() || LensFlareQuality == ELensFlareQuality::Disabled)
	{
		return FRenderingCompositeOutputRef();
	}

	FRenderingCompositeOutputRef LensFlareInput = SceneColorDownsampleChain[static_cast<uint32>(ELensFlareQuality::High) - static_cast<uint32>(LensFlareQuality)];

	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(
		new(FMemStack::Get()) TRCPassForRDG<2, 1>(
			[bCompositeWithBloom](FRenderingCompositePass* InPass, FRenderingCompositePassContext& InContext)
	{
		const FFinalPostProcessSettings& PostProcessSettings = InContext.View.FinalPostProcessSettings;

		FRDGBuilder GraphBuilder(InContext.RHICmdList);

		FRDGTextureRef BloomTexture = InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input0, TEXT("BloomInput"));
		const FIntRect BloomViewRect = InContext.GetDownsampledSceneColorViewRectFromInputExtent(BloomTexture->Desc.Extent);

		FRDGTextureRef FlareTexture = InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input1, TEXT("FlareInput"));
		const FIntRect FlareViewRect = InContext.GetDownsampledSceneColorViewRectFromInputExtent(FlareTexture->Desc.Extent);

		FRHITexture* BokehTextureRHI = GWhiteTexture->TextureRHI;

		if (GEngine->DefaultBokehTexture)
		{
			FTextureResource* BokehTextureResource = GEngine->DefaultBokehTexture->Resource;

			if (BokehTextureResource && BokehTextureResource->TextureRHI)
			{
				BokehTextureRHI = BokehTextureResource->TextureRHI;
			}
		}

		if (PostProcessSettings.LensFlareBokehShape)
		{
			FTextureResource* BokehTextureResource = PostProcessSettings.LensFlareBokehShape->Resource;

			if (BokehTextureResource && BokehTextureResource->TextureRHI)
			{
				BokehTextureRHI = BokehTextureResource->TextureRHI;
			}
		}

		FLensFlareInputs Inputs;
		Inputs.BloomTexture = BloomTexture;
		Inputs.BloomViewRect = BloomViewRect;
		Inputs.FlareTexture = FlareTexture;
		Inputs.FlareViewRect = FlareViewRect;
		Inputs.BokehShapeTexture = BokehTextureRHI;
		Inputs.TintColorsPerFlare = PostProcessSettings.LensFlareTints;
		Inputs.TintColor = PostProcessSettings.LensFlareTint;
		Inputs.BokehSizePercent = PostProcessSettings.LensFlareBokehSize;
		Inputs.Intensity = PostProcessSettings.LensFlareIntensity;
		Inputs.Threshold = PostProcessSettings.LensFlareThreshold;
		Inputs.bCompositeWithBloom = bCompositeWithBloom;

		FRDGTextureRef OutputTexture = AddLensFlaresPass(GraphBuilder, FScreenPassViewInfo(InContext.View), Inputs);

		if (OutputTexture)
		{
			InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, OutputTexture);
		}

		GraphBuilder.Execute();
	}));

	Pass->SetInput(ePId_Input0, BloomInput);
	Pass->SetInput(ePId_Input1, LensFlareInput);
	return Pass;
}