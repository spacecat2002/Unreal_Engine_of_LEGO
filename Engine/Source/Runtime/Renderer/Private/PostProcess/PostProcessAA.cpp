// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PostProcess/PostProcessAA.h"

BEGIN_SHADER_PARAMETER_STRUCT(FFXAAParameters, )
	SHADER_PARAMETER_STRUCT(FScreenPassTextureInput, Input)
	SHADER_PARAMETER(FVector4, fxaaConsoleRcpFrameOpt)
	SHADER_PARAMETER(FVector4, fxaaConsoleRcpFrameOpt2)
	SHADER_PARAMETER(float, fxaaQualitySubpix)
	SHADER_PARAMETER(float, fxaaQualityEdgeThreshold)
	SHADER_PARAMETER(float, fxaaQualityEdgeThresholdMin)
	SHADER_PARAMETER(float, fxaaConsoleEdgeSharpness)
	SHADER_PARAMETER(float, fxaaConsoleEdgeThreshold)
	SHADER_PARAMETER(float, fxaaConsoleEdgeThresholdMin)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FFXAAVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFXAAVS);
	// FDrawRectangleParameters is filled by DrawScreenPass.
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FFXAAVS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	using FParameters = FFXAAParameters;
};

IMPLEMENT_GLOBAL_SHADER(FFXAAVS, "/Engine/Private/FXAAShader.usf", "FxaaVS", SF_Vertex);

class FFXAAPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFXAAPS);
	SHADER_USE_PARAMETER_STRUCT(FFXAAPS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	class FQualityDimension : SHADER_PERMUTATION_ENUM_CLASS("FXAA_PRESET", EFXAAQuality);
	using FPermutationDomain = TShaderPermutationDomain<FQualityDimension>;
	using FParameters = FFXAAParameters;
};

IMPLEMENT_GLOBAL_SHADER(FFXAAPS, "/Engine/Private/FXAAShader.usf", "FxaaPS", SF_Pixel);

EFXAAQuality GetFXAAQuality()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessAAQuality"));

	return (EFXAAQuality)FMath::Clamp(CVar->GetValueOnAnyThread(), (int32)EFXAAQuality::Q0, (int32)EFXAAQuality::Q5);
}

FRDGTextureRef AddFXAAPass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureRef SceneColorTexture,
	FIntRect SceneColorViewRect,
	EFXAAQuality Quality)
{
	check(SceneColorTexture);
	check(!SceneColorViewRect.IsEmpty());
	check(Quality != EFXAAQuality::MAX);

	const FViewInfo& View = ScreenPassView.View;

	FRDGTextureRef OutputTexture = nullptr;
	FVector2D InverseExtent;

	{
		FRDGTextureDesc OutputDesc = SceneColorTexture->Desc;
		OutputDesc.Reset();
		OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FXAA"));

		InverseExtent = FVector2D(1.0f / (float)OutputDesc.Extent.X, 1.0f / (float)OutputDesc.Extent.Y);
	}

	const FScreenPassTextureViewport Viewport(SceneColorViewRect, SceneColorTexture);

	FRHISamplerState* BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FFXAAParameters* PassParameters = GraphBuilder.AllocParameters<FFXAAParameters>();
	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ScreenPassView.GetOverwriteLoadAction());
	PassParameters->Input = GetScreenPassTextureInput(Viewport, SceneColorTexture, BilinearClampSampler);

	{
		float N = 0.5f;
		FVector4 Value(-N * InverseExtent.X, -N * InverseExtent.Y, N * InverseExtent.X, N * InverseExtent.Y);
		PassParameters->fxaaConsoleRcpFrameOpt = Value;
	}

	{
		float N = 2.0f;
		FVector4 Value(-N * InverseExtent.X, -N * InverseExtent.Y, N * InverseExtent.X, N * InverseExtent.Y);
		PassParameters->fxaaConsoleRcpFrameOpt2 = Value;
	}

	PassParameters->fxaaQualitySubpix = 0.75f;
	PassParameters->fxaaQualityEdgeThreshold = 0.166f;
	PassParameters->fxaaQualityEdgeThresholdMin = 0.0833f;
	PassParameters->fxaaConsoleEdgeSharpness = 8.0f;
	PassParameters->fxaaConsoleEdgeThreshold = 0.125f;
	PassParameters->fxaaConsoleEdgeThresholdMin = 0.05f;

	FFXAAPS::FPermutationDomain PixelPermutationVector;
	PixelPermutationVector.Set<FFXAAPS::FQualityDimension>(Quality);

	TShaderMapRef<FFXAAVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FFXAAPS> PixelShader(View.ShaderMap, PixelPermutationVector);

	const FScreenPassDrawInfo ScreenPassDraw(*VertexShader, *PixelShader);

	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("FXAA %dx%d (PS)", Viewport.Rect.Width(), Viewport.Rect.Height()),
		ScreenPassView,
		Viewport,
		Viewport,
		ScreenPassDraw,
		PassParameters,
		[VertexShader, PixelShader, PassParameters](FRHICommandList& RHICmdList)
	{
		SetShaderParameters(RHICmdList, *VertexShader, VertexShader->GetVertexShader(), *PassParameters);
		SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), *PassParameters);
	});

	return OutputTexture;
}