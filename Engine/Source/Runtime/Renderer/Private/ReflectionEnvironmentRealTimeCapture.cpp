// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Functionality for capturing and pre-filtering a sky env map in real time.
=============================================================================*/

#include "ReflectionEnvironmentCapture.h"
#include "ClearQuad.h"
#include "ScenePrivate.h"
#include "RenderGraphUtils.h"
#include "VolumetricCloudRendering.h"
#include "VolumetricCloudProxy.h"


extern float GReflectionCaptureNearPlane;


DECLARE_GPU_STAT(CaptureConvolveSkyEnvMap);



class FDownsampleCubeFaceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDownsampleCubeFaceCS);
	SHADER_USE_PARAMETER_STRUCT(FDownsampleCubeFaceCS, FGlobalShader);

	static const uint32 ThreadGroupSize = 8;

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MipIndex)
		SHADER_PARAMETER(uint32, NumMips)
		SHADER_PARAMETER(int32, CubeFace)
		SHADER_PARAMETER(int32, FaceThreadGroupSize)
		SHADER_PARAMETER(FIntPoint, ValidDispatchCoord)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(TextureCube, SourceCubemapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTextureMipColor)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5; }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("USE_COMPUTE"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FDownsampleCubeFaceCS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "DownsampleCS", SF_Compute);


class FConvolveSpecularFaceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FConvolveSpecularFaceCS);
	SHADER_USE_PARAMETER_STRUCT(FConvolveSpecularFaceCS, FGlobalShader);

	static const uint32 ThreadGroupSize = 8;

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MipIndex)
		SHADER_PARAMETER(uint32, NumMips)
		SHADER_PARAMETER(int32, CubeFace)
		SHADER_PARAMETER(int32, FaceThreadGroupSize)
		SHADER_PARAMETER(FIntPoint, ValidDispatchCoord)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(TextureCube, SourceCubemapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTextureMipColor)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5; }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("USE_COMPUTE"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FConvolveSpecularFaceCS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "FilterCS", SF_Compute);


class FComputeSkyEnvMapDiffuseIrradianceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FComputeSkyEnvMapDiffuseIrradianceCS);
	SHADER_USE_PARAMETER_STRUCT(FComputeSkyEnvMapDiffuseIrradianceCS, FGlobalShader);

	// 8*8=64 threads in a group.
	// Each thread uses 4*7*RGB sh float => 84 bytes shared group memory. 
	// 64 * 84 = 5376 bytes which fits dx11 16KB shared memory limitation. 6144 with vector alignement in shared memory and it still fits
	// Low occupancy on a single CU.
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(TextureCube, SourceCubemapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
		SHADER_PARAMETER_UAV(RWStructuredBuffer, OutIrradianceEnvMapSH)
		SHADER_PARAMETER(float, UniformSampleSolidAngle)
		END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5; }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), ThreadGroupSizeY);
		OutEnvironment.SetDefine(TEXT("SHADER_DIFFUSE_TO_SH"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FComputeSkyEnvMapDiffuseIrradianceCS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "ComputeSkyEnvMapDiffuseIrradianceCS", SF_Compute);


void FScene::AllocateAndCaptureFrameSkyEnvMap(
	FRHICommandListImmediate& RHICmdList, FSceneRenderer& SceneRenderer, FViewInfo& MainView, 
	bool bShouldRenderSkyAtmosphere, bool bShouldRenderVolumetricCloud)
{
	check(SkyLight && SkyLight->bRealTimeCaptureEnabled && !SkyLight->bHasStaticLighting);

	SCOPED_DRAW_EVENT(RHICmdList, CaptureConvolveSkyEnvMap);
	SCOPED_GPU_STAT(RHICmdList, CaptureConvolveSkyEnvMap);

	const uint32 CubeWidth = SkyLight->CaptureCubeMapResolution;
	const uint32 CubeMipCount = FMath::CeilLogTwo(CubeWidth) + 1;

	// Make a snapshot we are going to use for the 6 cubemap faces and set it up.
	FViewInfo& CubeView = *MainView.CreateSnapshot();
	CubeView.FOV = 90.0f;
	CubeView.PreExposure = 1.0f;

	FViewMatrices::FMinimalInitializer SceneCubeViewInitOptions;
	SceneCubeViewInitOptions.ConstrainedViewRect = FIntRect(FIntPoint(0, 0), FIntPoint(CubeWidth, CubeWidth));

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FBox VolumeBounds[TVC_MAX];
	CubeView.CachedViewUniformShaderParameters = MakeUnique<FViewUniformShaderParameters>();
	CubeView.SetupUniformBufferParameters(
		SceneContext,
		VolumeBounds,
		TVC_MAX,
		*CubeView.CachedViewUniformShaderParameters);

	const FMatrix CubeProjectionMatrix = GetCubeProjectionMatrix(CubeView.FOV * 0.5f, (float)CubeWidth, GReflectionCaptureNearPlane);
	CubeView.UpdateProjectionMatrix(CubeProjectionMatrix);

	FPooledRenderTargetDesc SkyCubeTexDesc = FPooledRenderTargetDesc::CreateCubemapDesc(CubeWidth, 
		PF_FloatR11G11B10, FClearValueBinding::Black, TexCreate_TargetArraySlicesIndependently,
		TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable, false, 1, CubeMipCount);

	GRenderTargetPool.FindFreeElement(RHICmdList, SkyCubeTexDesc, CapturedSkyRenderTarget, TEXT("CapturedSkyRenderTarget"), true, ERenderTargetTransience::Transient);
	GRenderTargetPool.FindFreeElement(RHICmdList, SkyCubeTexDesc, ConvolvedSkyRenderTarget, TEXT("ConvolvedSkyRenderTarget"), true, ERenderTargetTransience::Transient);

	if (bShouldRenderSkyAtmosphere)
	{
		FRDGBuilder GraphBuilder(RHICmdList);// , RDG_EVENT_NAME("CaptureConvolveSkyEnvMap"));

		// TODO dispatch the sky mesh if View->bSceneHasSkyMaterial is true instead of simple sky rendering

		FSkyAtmosphereRenderSceneInfo& SkyInfo = *GetSkyAtmosphereSceneInfo();
		const FSkyAtmosphereSceneProxy& SkyAtmosphereSceneProxy = SkyInfo.GetSkyAtmosphereSceneProxy();

		FRDGTextureRef CapturedSkyTex = GraphBuilder.RegisterExternalTexture(CapturedSkyRenderTarget, TEXT("CapturedSkyRenderTarget"));
		FRDGTextureRef BlackDummy2dTex = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		FRDGTextureRef BlackDummy3dTex = GraphBuilder.RegisterExternalTexture(GSystemTextures.VolumetricBlackDummy);

		SkyAtmosphereRenderContext SkyRC;

		// Global data constant between faces
		const FAtmosphereSetup& AtmosphereSetup = SkyAtmosphereSceneProxy.GetAtmosphereSetup();
		SkyRC.bMultiScattering = SkyAtmosphereSceneProxy.IsMultiScatteringEnabled();
		SkyRC.bFastSky = false;
		SkyRC.bFastAerialPerspective = false;
		SkyRC.bFastAerialPerspectiveDepthTest = false;
		SkyRC.bSecondAtmosphereLightEnabled = IsSecondAtmosphereLightEnabled();

		SkyRC.bShouldSampleOpaqueShadow = false;	// No atmospheric light shadow
		SkyRC.bUseDepthBoundTestIfPossible = false;
		SkyRC.bForceRayMarching = true;				// We do not have any valid view LUT
		SkyRC.bDisableDepthRead = true;
		SkyRC.bDisableBlending = true;

		SkyRC.TransmittanceLut = GraphBuilder.RegisterExternalTexture(SkyInfo.GetTransmittanceLutTexture());
		SkyRC.MultiScatteredLuminanceLut = GraphBuilder.RegisterExternalTexture(SkyInfo.GetMultiScatteredLuminanceLutTexture());

		CloudRenderContext CloudRC;
		if (bShouldRenderVolumetricCloud)
		{
			FVolumetricCloudRenderSceneInfo& CloudInfo = *GetVolumetricCloudSceneInfo();
			FVolumetricCloudSceneProxy& CloudSceneProxy = CloudInfo.GetVolumetricCloudSceneProxy();

			if (CloudSceneProxy.GetCloudVolumeMaterial())
			{
				FMaterialRenderProxy* CloudVolumeMaterialProxy = CloudSceneProxy.GetCloudVolumeMaterial()->GetRenderProxy();
				CloudRC.CloudInfo = &CloudInfo;
				CloudRC.CloudVolumeMaterialProxy = CloudVolumeMaterialProxy;
				CloudRC.SceneDepthZ = GSystemTextures.MaxFP16Depth;

				CloudRC.MainView = &CubeView;

				CloudRC.bShouldViewRenderVolumetricRenderTarget = false;
				CloudRC.bIsReflectionRendering = true;
				CloudRC.bSkipAtmosphericLightShadowmap = true;
			}
			else
			{
				bShouldRenderVolumetricCloud = false; // Disable cloud rendering
			}
		}

		AtmosphereSetup.ComputeViewData(SkyLight->CapturePosition, CubeView.CachedViewUniformShaderParameters->SkyWorldCameraOrigin, CubeView.CachedViewUniformShaderParameters->SkyPlanetCenterAndViewHeight);


		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			SkyRC.RenderTargets[0] = FRenderTargetBinding(CapturedSkyTex, ERenderTargetLoadAction::ENoAction, 0, CubeFace);

			const FMatrix CubeViewRotationMatrix = CalcCubeFaceViewRotationMatrix((ECubeFace)CubeFace);

			SceneCubeViewInitOptions.ViewRotationMatrix = CubeViewRotationMatrix;
			SceneCubeViewInitOptions.ViewOrigin = SkyLight->CapturePosition;
			SceneCubeViewInitOptions.ProjectionMatrix = CubeProjectionMatrix;
			FViewMatrices CubeViewMatrices = FViewMatrices(SceneCubeViewInitOptions);
			CubeView.SetupCommonViewUniformBufferParameters(
				*CubeView.CachedViewUniformShaderParameters,
				FIntPoint(CubeWidth, CubeWidth),
				1,
				FIntRect(FIntPoint(0, 0), FIntPoint(CubeWidth, CubeWidth)),
				CubeViewMatrices,
				CubeViewMatrices);

			SkyRC.ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(*CubeView.CachedViewUniformShaderParameters, UniformBuffer_SingleDraw);
			SkyRC.ViewMatrices = &CubeViewMatrices;

			SkyRC.SkyAtmosphereViewLutTexture = BlackDummy2dTex;
			SkyRC.SkyAtmosphereCameraAerialPerspectiveVolume = BlackDummy3dTex;

			SkyRC.Viewport = FIntRect(FIntPoint(0, 0), FIntPoint(CubeWidth, CubeWidth));
			SkyRC.bLightDiskEnabled = false;
			SkyRC.bRenderSkyPixel = true;
			SkyRC.AerialPerspectiveStartDepthInCm = 0.01f;
			SkyRC.NearClippingDistance = 0.01f;
			SkyRC.FeatureLevel = FeatureLevel;

			//SkyRC.LightShadowShaderParams0UniformBuffer = nullptr;
			//SkyRC.LightShadowShaderParams1UniformBuffer = nullptr;

			SkyRC.bShouldSampleCloudShadow = false;
			SkyRC.VolumetricCloudShadowMap = BlackDummy2dTex;

			SceneRenderer.RenderSkyAtmosphereInternal(GraphBuilder, SkyRC);

			if (bShouldRenderVolumetricCloud)
			{
				CloudRC.ViewUniformBuffer = SkyRC.ViewUniformBuffer;

				CloudRC.RenderTargets[0] = SkyRC.RenderTargets[0];
				//	CloudRC.RenderTargets[1] = Null target will skip export

				SetVolumeShadowingDefaultShaderParameters(CloudRC.LightShadowShaderParams0);

				CloudRC.VolumetricCloudShadowTexture = CubeView.VolumetricCloudShadowMap.IsValid() ? GraphBuilder.RegisterExternalTexture(CubeView.VolumetricCloudShadowMap) : BlackDummy2dTex;

				SceneRenderer.RenderVolumetricCloudsInternal(GraphBuilder, CloudRC);
			}

		}

		GraphBuilder.Execute();
		GraphBuilder.QueueTextureExtraction(CapturedSkyTex, &CapturedSkyRenderTarget);
	}
	else
	{
		FRDGBuilder GraphBuilder(RHICmdList);// , RDG_EVENT_NAME("ClearCapturedSkyRenderTarget"));
		FRDGTextureRef CapturedSkyTex = GraphBuilder.RegisterExternalTexture(CapturedSkyRenderTarget, TEXT("CapturedSkyRenderTarget"));

		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			FRenderTargetParameters* Parameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
			Parameters->RenderTargets[0] = FRenderTargetBinding(CapturedSkyTex, ERenderTargetLoadAction::ENoAction, 0, CubeFace);

			FLinearColor ClearColor = FLinearColor::Black;
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ClearCapturedSkyRenderTarget"),
				Parameters,
				ERDGPassFlags::Raster,
				[Parameters, ClearColor](FRHICommandList& RHICmdList)
				{
					DrawClearQuad(RHICmdList, ClearColor);
				});
		}
		GraphBuilder.Execute();
		GraphBuilder.QueueTextureExtraction(CapturedSkyTex, &CapturedSkyRenderTarget);
	}

	// CreateCubeMips 
#if 1
	{
		FRDGBuilder GraphBuilder(RHICmdList);// , RDG_EVENT_NAME("GenerateMipChain"));
		FRDGTextureRef RDGDownSampledCube = GraphBuilder.RegisterExternalTexture(CapturedSkyRenderTarget, TEXT("CapturedSkyRenderTarget"));

		FDownsampleCubeFaceCS::FPermutationDomain PermutationVector;
		TShaderMapRef<FDownsampleCubeFaceCS> ComputeShader(GetGlobalShaderMap(FeatureLevel), PermutationVector);

		for (uint32 MipIndex = 1; MipIndex < CubeMipCount; MipIndex++)
		{
			const uint32 MipResolution = 1 << (CubeMipCount - MipIndex - 1);
			FRDGTextureSRVRef RDGDownSampledCubeSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(RDGDownSampledCube, MipIndex - 1)); // slice/face selection is useless so remove from CreateForMipLevel

			FDownsampleCubeFaceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsampleCubeFaceCS::FParameters>();
			PassParameters->MipIndex = MipIndex;
			PassParameters->NumMips = CubeMipCount;
			PassParameters->CubeFace = 0; // unused
			PassParameters->ValidDispatchCoord = FIntPoint(MipResolution, MipResolution);
			PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Point>::GetRHI();

			PassParameters->SourceCubemapTexture = RDGDownSampledCubeSRV;
			PassParameters->OutTextureMipColor  = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RDGDownSampledCube, MipIndex));

			FIntVector NumGroups = FIntVector::DivideAndRoundUp(FIntVector(MipResolution, MipResolution, 1), FIntVector(FDownsampleCubeFaceCS::ThreadGroupSize, FDownsampleCubeFaceCS::ThreadGroupSize, 1));

			// The groupd size per face with padding
			PassParameters->FaceThreadGroupSize = NumGroups.X * FDownsampleCubeFaceCS::ThreadGroupSize;

			// We are going to dispatch once for all faces 
			NumGroups.X *= 6;

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("MipGen"), ComputeShader, PassParameters, NumGroups);
		}

		GraphBuilder.Execute();
	}

	FSceneRenderTargetItem& DownSampledCube = CapturedSkyRenderTarget->GetRenderTargetItem();
	FSceneRenderTargetItem& ConvolvedCube = ConvolvedSkyRenderTarget->GetRenderTargetItem();
	RHICmdList.CopyToResolveTarget(DownSampledCube.TargetableTexture, DownSampledCube.ShaderResourceTexture, FResolveParams());

#else
	FSceneRenderTargetItem& DownSampledCube = CapturedSkyRenderTarget->GetRenderTargetItem();
	FSceneRenderTargetItem& ConvolvedCube = ConvolvedSkyRenderTarget->GetRenderTargetItem();
	// Using the pixel path because compute path above cannot be achieved.
	// TODO convert to RDG and not use GetImmediateCommandList.
	extern void CreateCubeMips(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 NumMips, FSceneRenderTargetItem& Cubemap);
	CreateCubeMips(RHICmdList, FeatureLevel, CubeMipCount, DownSampledCube);
	RHICmdList.CopyToResolveTarget(DownSampledCube.TargetableTexture, DownSampledCube.ShaderResourceTexture, FResolveParams());
#endif

	// Specular convolution
#if 1
	{
		FRDGBuilder GraphBuilder(RHICmdList);// , RDG_EVENT_NAME("ConvolveSpecular"));
		FRDGTextureRef RDGDownSampledCube = GraphBuilder.RegisterExternalTexture(CapturedSkyRenderTarget, TEXT("CapturedSkyRenderTarget"));
		FRDGTextureRef RDGConvolvedCube = GraphBuilder.RegisterExternalTexture(ConvolvedSkyRenderTarget, TEXT("CapturedSkyRenderTarget"));

		FRDGTextureSRVRef RDGDownSampledCubeSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(RDGDownSampledCube));

		FDownsampleCubeFaceCS::FPermutationDomain PermutationVector;
		TShaderMapRef<FConvolveSpecularFaceCS> ComputeShader(GetGlobalShaderMap(FeatureLevel), PermutationVector);
		for (uint32 MipIndex = 0; MipIndex < CubeMipCount; MipIndex++)
		{
			const uint32 MipResolution = 1 << (CubeMipCount - MipIndex - 1);

			FConvolveSpecularFaceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FConvolveSpecularFaceCS::FParameters>();
			PassParameters->MipIndex = MipIndex;
			PassParameters->NumMips = CubeMipCount;
			PassParameters->CubeFace = 0; // unused
			PassParameters->ValidDispatchCoord = FIntPoint(MipResolution, MipResolution);
			PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Point>::GetRHI();

			PassParameters->SourceCubemapTexture = RDGDownSampledCubeSRV;
			PassParameters->OutTextureMipColor  = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RDGConvolvedCube, MipIndex));

			FIntVector NumGroups = FIntVector::DivideAndRoundUp(FIntVector(MipResolution, MipResolution, 1), FIntVector(FConvolveSpecularFaceCS::ThreadGroupSize, FConvolveSpecularFaceCS::ThreadGroupSize, 1));

			// The groupd size per face with padding
			PassParameters->FaceThreadGroupSize = NumGroups.X * FConvolveSpecularFaceCS::ThreadGroupSize;

			// We are going to dispatch once for all faces 
			NumGroups.X *= 6;

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Convolve"), ComputeShader, PassParameters, NumGroups);
		}

		GraphBuilder.Execute();
	}
	RHICmdList.CopyToResolveTarget(ConvolvedCube.TargetableTexture, ConvolvedCube.ShaderResourceTexture, FResolveParams());
#else
	extern void FilterCubeMap(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 NumMips, FSceneRenderTargetItem& DownSampledCube, FSceneRenderTargetItem& FilteredCube);
	FilterCubeMap(RHICmdList, FeatureLevel, CubeMipCount, DownSampledCube, ConvolvedCube);
	RHICmdList.CopyToResolveTarget(ConvolvedCube.TargetableTexture, ConvolvedCube.ShaderResourceTexture, FResolveParams());
#endif


	// ComputeDiffuseIrradiance using N uniform samples
	{
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, SkyIrradianceEnvironmentMap.UAV);

		FRDGBuilder GraphBuilder(RHICmdList);// , RDG_EVENT_NAME("ComputeDiffuseIrradiance"));

		FRDGTextureRef CapturedSkyTex = GraphBuilder.RegisterExternalTexture(CapturedSkyRenderTarget, TEXT("CapturedSkyRenderTarget"));
		FRDGTextureSRVRef DownSampledCubeSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(CapturedSkyTex));

		TShaderMapRef<FComputeSkyEnvMapDiffuseIrradianceCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));

		const float SampleCount = FComputeSkyEnvMapDiffuseIrradianceCS::ThreadGroupSizeX * FComputeSkyEnvMapDiffuseIrradianceCS::ThreadGroupSizeY;
		const float UniformSampleSolidAngle = 4.0f * PI / SampleCount; // uniform distribution

		FComputeSkyEnvMapDiffuseIrradianceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeSkyEnvMapDiffuseIrradianceCS::FParameters>();
		PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->SourceCubemapTexture = DownSampledCubeSRV;
		PassParameters->OutIrradianceEnvMapSH = SkyIrradianceEnvironmentMap.UAV;
		PassParameters->UniformSampleSolidAngle = UniformSampleSolidAngle;
		// TODO adapt the sampled MipLevel according to resolution, leveraging the fact that we have updated the full mip chain, to reduce flickering.

		const FIntVector NumGroups = FIntVector(1, 1, 1);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ComputeSkyEnvMapDiffuseIrradianceCS"), ComputeShader, PassParameters, NumGroups);
		GraphBuilder.Execute();

		// This buffer is now going to be read for rendering.
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SkyIrradianceEnvironmentMap.UAV);
	}
}


