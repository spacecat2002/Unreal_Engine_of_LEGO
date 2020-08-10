// Copyright Epic Games, Inc. All Rights Reserved.

#include "MobileDeferredShadingPass.h"
#include "SceneView.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"

class FMobileDeferredShadingPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileDeferredShadingPS, Global);

private:
	LAYOUT_FIELD(FShaderUniformBufferParameter, MobileDirectionLightBufferParam);
	LAYOUT_FIELD(FShaderUniformBufferParameter, ReflectionParameter);

public:
	class FApplySkyReflection	: SHADER_PERMUTATION_BOOL("APPLY_SKY_REFLECTION");
	class FApplyCSM				: SHADER_PERMUTATION_BOOL("APPLY_CSM");
	class FShadowQuality		: SHADER_PERMUTATION_INT("MOBILE_SHADOW_QUALITY", 4);
	using FPermutationDomain = TShaderPermutationDomain< FApplySkyReflection, FApplyCSM, FShadowQuality>;

	/** Default constructor. */
	FMobileDeferredShadingPS() {}

	/** Initialization constructor. */
	FMobileDeferredShadingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MobileDirectionLightBufferParam.Bind(Initializer.ParameterMap, FMobileDirectionalLightShaderParameters::StaticStructMetadata.GetShaderVariableName());
		ReflectionParameter.Bind(Initializer.ParameterMap, FMobileReflectionCaptureShaderParameters::StaticStructMetadata.GetShaderVariableName());
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene& Scene, const FSceneView& View)
	{
		FRHIPixelShader* ShaderRHI = RHICmdList.GetBoundPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SetUniformBufferParameter(RHICmdList, ShaderRHI, MobileDirectionLightBufferParam, Scene.UniformBuffers.MobileDirectionalLightUniformBuffers[1]);
		SetUniformBufferParameter(RHICmdList, ShaderRHI, ReflectionParameter, Scene.UniformBuffers.MobileSkyReflectionUniformBuffer);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_MOBILE_SHADOWCASCADES)), GetMobileMaxShadowCascades());
	}

	static FPermutationDomain RemapPermutationVector(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FApplyCSM>() == false)
		{
			PermutationVector.Set<FShadowQuality>(0);
		}
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		// Compile out the shader if this permutation gets remapped.
		if (RemapPermutationVector(PermutationVector) != PermutationVector)
		{
			return false;
		}
		
		return true;
	}

	static FPermutationDomain BuildPermutationVector(const FViewInfo& View)
	{
		bool bApplySky = View.Family->EngineShowFlags.SkyLighting;
		int32 ShadowQuality = (int32)GetShadowQuality();
				
		FPermutationDomain PermutationVector;
		PermutationVector.Set<FMobileDeferredShadingPS::FApplySkyReflection>(bApplySky);
		PermutationVector.Set<FMobileDeferredShadingPS::FApplyCSM>(ShadowQuality > 0);
		PermutationVector.Set<FMobileDeferredShadingPS::FShadowQuality>(FMath::Clamp(ShadowQuality - 1, 0, 3));
		return PermutationVector;
	}
};

IMPLEMENT_SHADER_TYPE(, FMobileDeferredShadingPS, TEXT("/Engine/Private/MobileDeferredShading.usf"), TEXT("MobileDeferredShadingPS"), SF_Pixel);

void MobileDeferredShadingPass(FRHICommandListImmediate& RHICmdList, const FScene& Scene, const FViewInfo& View)
{
	SCOPED_DRAW_EVENT(RHICmdList, MobileDeferredShading);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FUniformBufferRHIRef PassUniformBuffer = CreateMobileSceneTextureUniformBuffer(RHICmdList);
	FUniformBufferStaticBindings GlobalUniformBuffers(PassUniformBuffer);
	SCOPED_UNIFORM_BUFFER_GLOBAL_BINDINGS(RHICmdList, GlobalUniformBuffers);
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	// Add to emissive in SceneColor
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	// Shade only MSM_DefaultLit pixels
	uint8 StencilRef = (MSM_DefaultLit & 0x0f);
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
										false, CF_Always,
										true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,		
										false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
										0x0f, 0x00>::GetRHI(); // 4 bits for shading models
	
	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
	FMobileDeferredShadingPS::FPermutationDomain PermutationVector = FMobileDeferredShadingPS::BuildPermutationVector(View);
	TShaderMapRef<FMobileDeferredShadingPS> PixelShader(View.ShaderMap, PermutationVector);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	PixelShader->SetParameters(RHICmdList, Scene, View);
	RHICmdList.SetStencilRef(StencilRef);
			
	const FIntPoint TargetSize = SceneContext.GetBufferSizeXY();
	RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);

	DrawRectangle(
		RHICmdList,
		0, 0,
		TargetSize.X, TargetSize.Y,
		0, 0,
		TargetSize.X, TargetSize.Y,
		TargetSize,
		TargetSize,
		VertexShader,
		EDRF_Default);
}