// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricCloudRendering.cpp
=============================================================================*/

#include "VolumetricCloudRendering.h"
#include "Components/VolumetricCloudComponent.h"
#include "VolumetricCloudProxy.h"
#include "DeferredShadingRenderer.h"
#include "PixelShaderUtils.h"
#include "RenderCore/Public/RenderGraphUtils.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "SkyAtmosphereRendering.h"
#include "VolumeLighting.h"
#include "DynamicPrimitiveDrawing.h"
#include "GpuDebugRendering.h"
#include "CanvasTypes.h"
#include "RenderTargetTemp.h"
#include "VolumetricRenderTarget.h"
#include "BlueNoise.h"


//PRAGMA_DISABLE_OPTIMIZATION

////////////////////////////////////////////////////////////////////////// Cloud rendering and tracing

// The runtime ON/OFF toggle
static TAutoConsoleVariable<int32> CVarVolumetricCloud(
	TEXT("r.VolumetricCloud"), 1,
	TEXT("VolumetricCloud components are rendered when this is not 0, otherwise ignored."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarVolumetricCloudPerSampleSkyAtmosphereTransmittance(
	TEXT("r.VolumetricCloud.PerSampleSkyAtmosphereTransmittance"), 0,
	TEXT("This is necessary to get correct colorisation on clouds when viewed from space."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarVolumetricCloudTracingStartMaxDistance(
	TEXT("r.VolumetricCloud.TracingStartMaxDistance"), 350.0f,
	TEXT("The maximum distance (kilometers) of the volumetric surface before which we will accept to start tracing."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricCloudTracingMaxDistance(
	TEXT("r.VolumetricCloud.TracingMaxDistance"), 50.0f,
	TEXT("The maximum distance (kilometers) that will be traced inside the cloud layer."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricCloudSampleCountMax(
	TEXT("r.VolumetricCloud.SampleCountMax"), 32,
	TEXT("The maximum number of samples taken while ray marching primary rays."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricCloudReflectionSampleCountMax(
	TEXT("r.VolumetricCloud.ReflectionSampleCountMax"), 6,
	TEXT("The maximum number of samples taken while ray marching primary rays in reflections."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricCloudDistanceToSampleCountMax(
	TEXT("r.VolumetricCloud.DistanceToSampleCountMax"), 15.0f,
	TEXT("The number of ray marching samples will span 0 to SampleCountMax from 0 to DistanceToSampleCountMax (kilometers). After that it is capped at SampleCountMax."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

////////////////////////////////////////////////////////////////////////// Shadow tracing

static TAutoConsoleVariable<int32> CVarVolumetricCloudShadowSampleCountMax(
	TEXT("r.VolumetricCloud.Shadow.SampleCountMax"), 10,
	TEXT("The maximum number of samples taken while ray marching shadow rays."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricCloudReflectionShadowSampleCountMax(
	TEXT("r.VolumetricCloud.Shadow.ReflectionSampleCountMax"), 4,
	TEXT("The maximum number of samples taken while ray marching shadow rays in reflections."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricCloudShadowTracingMaxDistance(
	TEXT("r.VolumetricCloud.Shadow.TracingMaxDistance"), 10.0f,
	TEXT("The maximum distance (kilometers) that will be traced inside the cloud layer for shadow rays."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricCloudShadowSampleAtmosphericLightShadowmap(
	TEXT("r.VolumetricCloud.Shadow.SampleAtmosphericLightShadowmap"), 0,
	TEXT("Enable the sampling of atmospheric lights shadow map in order to produce volumetric shadows."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

////////////////////////////////////////////////////////////////////////// Ground lighting

static TAutoConsoleVariable<int32> CVarVolumetricCloudGroundLightingTextureResolution(
	TEXT("r.VolumetricCloud.GroundLighting.TextureResolution"), 256,
	TEXT("The resolution of the texture storting occlusion information for the lighting coming from the ground."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricCloudGroundLightingSampleCount(
	TEXT("r.VolumetricCloud.GroundLighting.SampleCount"), 10,
	TEXT("The number of sample taken to evaluate ground lighting occlusion."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricCloudGroundLightingOverrideExtend(
	TEXT("r.VolumetricCloud.GroundLighting.OverrideExtent"), -1.0f,
	TEXT("The world space extent of the ground lighting occlusion texture can be overriden when this is greater than 0 (Kilometers)."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

////////////////////////////////////////////////////////////////////////// Cloud shadow map

static TAutoConsoleVariable<int32> CVarVolumetricCloudShadowMap(
	TEXT("r.VolumetricCloud.ShadowMap"), 0,
	TEXT("Enable / disable the shadow map."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricCloudShadowMapDebug(
	TEXT("r.VolumetricCloud.ShadowMap.Debug"), 0,
	TEXT("Print information to debug the cloud shadow map."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricCloudShadowMapResolution(
	TEXT("r.VolumetricCloud.ShadowMap.Resolution"), 256,
	TEXT("The resolution of the cloud shadow map."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricCloudShadowMapStrength(
	TEXT("r.VolumetricCloud.ShadowMap.Strength"), 1.0f,
	TEXT("The strenght of the cloud shadow."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricCloudShadowMapOverrideExtend(
	TEXT("r.VolumetricCloud.ShadowMap.OverrideExtent"), -1.0f,
	TEXT("The world space extent of the cloud shadow map around the camera in kilometers."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricCloudShadowFiltering(
	TEXT("r.VolumetricCloud.ShadowMap.Filtering"), 1,
	TEXT("Enable / disable the shadow map dilation/smoothing filter."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

////////////////////////////////////////////////////////////////////////// Lighting component controls

static TAutoConsoleVariable<int32> CVarVolumetricCloudEnableAerialPerspectiveSampling(
	TEXT("r.VolumetricCloud.EnableAerialPerspectiveSampling"), 1,
	TEXT("Enable/Disable the aerial perspective contribution on clouds."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarVolumetricCloudEnableDistantSkyLightSampling(
	TEXT("r.VolumetricCloud.EnableDistantSkyLightSampling"), 1,
	TEXT("Enable/Disable the distant sky light contribution on clouds."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarVolumetricCloudEnableAtmosphericLightsSampling(
	TEXT("r.VolumetricCloud.EnableAtmosphericLightsSampling"), 1,
	TEXT("Enable/Disable the atmospheric lights contribution on clouds."),
	ECVF_RenderThreadSafe);

////////////////////////////////////////////////////////////////////////// 

static TAutoConsoleVariable<int32> CVarVolumetricCloudDebugSampleCountMode(
	TEXT("r.VolumetricCloud.Debug.SampleCountMode"), 0,
	TEXT("Debug mode for per trace sample count."));

////////////////////////////////////////////////////////////////////////// 


static bool ShouldPipelineCompileVolumetricCloudShader(EShaderPlatform ShaderPlatform)
{
	// Requires SM5 or ES3_1 (GL/Vulkan) for compute shaders and volume textures support.
	return RHISupportsComputeShaders(ShaderPlatform);
}

bool ShouldRenderVolumetricCloud(const FScene* Scene, const FEngineShowFlags& EngineShowFlags)
{
	if (Scene && Scene->HasVolumetricCloud() ) //&& EngineShowFlags.VolumetricCloud) TODO apply 10810454 for clouds
	{
		const FVolumetricCloudRenderSceneInfo* VolumetricCloud = Scene->GetVolumetricCloudSceneInfo();
		check(VolumetricCloud);

		const bool ShadersCompiled = ShouldPipelineCompileVolumetricCloudShader(Scene->GetShaderPlatform());

		return ShadersCompiled && CVarVolumetricCloud.GetValueOnRenderThread() > 0 && Scene->AtmosphereLights[0]!=nullptr;
	}
	return false;
}

static int32 GetVolumetricCloudShadowMapResolution()
{
	return FMath::Max(128, CVarVolumetricCloudShadowMapResolution.GetValueOnAnyThread());
}


/*=============================================================================
	FVolumetricCloudRenderSceneInfo implementation.
=============================================================================*/


FVolumetricCloudRenderSceneInfo::FVolumetricCloudRenderSceneInfo(FVolumetricCloudSceneProxy& VolumetricCloudSceneProxyIn)
	:VolumetricCloudSceneProxy(VolumetricCloudSceneProxyIn)
{
}

FVolumetricCloudRenderSceneInfo::~FVolumetricCloudRenderSceneInfo()
{
}



/*=============================================================================
	FScene functions
=============================================================================*/



void FScene::AddVolumetricCloud(FVolumetricCloudSceneProxy* VolumetricCloudSceneProxy)
{
	check(VolumetricCloudSceneProxy);
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FAddVolumetricCloudCommand)(
		[Scene, VolumetricCloudSceneProxy](FRHICommandListImmediate& RHICmdList)
		{
			check(!Scene->VolumetricCloudStack.Contains(VolumetricCloudSceneProxy));
			Scene->VolumetricCloudStack.Push(VolumetricCloudSceneProxy);

			VolumetricCloudSceneProxy->RenderSceneInfo = new FVolumetricCloudRenderSceneInfo(*VolumetricCloudSceneProxy);

			// Use the most recently enabled VolumetricCloud
			Scene->VolumetricCloud = VolumetricCloudSceneProxy->RenderSceneInfo;
		} );
}

void FScene::RemoveVolumetricCloud(FVolumetricCloudSceneProxy* VolumetricCloudSceneProxy)
{
	check(VolumetricCloudSceneProxy);
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FRemoveVolumetricCloudCommand)(
		[Scene, VolumetricCloudSceneProxy](FRHICommandListImmediate& RHICmdList)
		{
			delete VolumetricCloudSceneProxy->RenderSceneInfo;
			Scene->VolumetricCloudStack.RemoveSingle(VolumetricCloudSceneProxy);

			if (Scene->VolumetricCloudStack.Num() > 0)
			{
				// Use the most recently enabled VolumetricCloud
				Scene->VolumetricCloud = Scene->VolumetricCloudStack.Last()->RenderSceneInfo;
			}
			else
			{
				Scene->VolumetricCloud = nullptr;
			}
		} );
}



/*=============================================================================
	VolumetricCloud rendering functions
=============================================================================*/



DECLARE_GPU_STAT(VolumetricCloud);
DECLARE_GPU_STAT(VolumetricCloudShadow);



FORCEINLINE bool IsVolumetricCloudMaterialSupported(const EShaderPlatform Platform)
{
	return GetMaxSupportedFeatureLevel(Platform) >= ERHIFeatureLevel::SM5;
}


FORCEINLINE bool IsMaterialCompatibleWithVolumetricCloud(const FMaterialShaderParameters& Material, const EShaderPlatform Platform)
{
	return IsVolumetricCloudMaterialSupported(Platform) && Material.MaterialDomain == MD_Volume;
}



//////////////////////////////////////////////////////////////////////////

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRenderVolumetricCloudGlobalParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FVolumetricCloudCommonShaderParameters, VolumetricCloud)
	SHADER_PARAMETER_TEXTURE(Texture2D, SceneDepthTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D, CloudGroundLightingTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D, ShadowTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, CloudBilinearTextureSampler)
	SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeShadowingShaderParametersGlobal0, Light0Shadow)
//	SHADER_PARAMETER_STRUCT(FBlueNoise, BlueNoise)
	SHADER_PARAMETER(FUintVector4, SubSetCoordToFullResolutionScaleBias)
	SHADER_PARAMETER(uint32, NoiseFrameIndexModPattern)
	SHADER_PARAMETER(uint32, VolumetricRenderTargetMode)
	SHADER_PARAMETER(uint32, SampleCountDebugMode)
	SHADER_PARAMETER(uint32, IsReflectionRendering)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRenderVolumetricCloudGlobalParameters, "RenderVolumetricCloudParameters");

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FVolumetricCloudCommonGlobalShaderParameters, "VolumetricCloudCommonParameters");

// When calling this, you still need to setup Light0Shadow yourself.
void SetupDefaultRenderVolumetricCloudGlobalParameters(FRenderVolumetricCloudGlobalParameters& VolumetricCloudParams, FVolumetricCloudRenderSceneInfo& CloudInfo)
{
	TRefCountPtr<IPooledRenderTarget> BlackDummy = GSystemTextures.BlackDummy;
	VolumetricCloudParams.VolumetricCloud = CloudInfo.GetVolumetricCloudCommonShaderParameters();
	VolumetricCloudParams.SceneDepthTexture = BlackDummy->GetRenderTargetItem().ShaderResourceTexture;
	VolumetricCloudParams.CloudGroundLightingTexture = BlackDummy->GetRenderTargetItem().ShaderResourceTexture;
	VolumetricCloudParams.ShadowTexture = BlackDummy->GetRenderTargetItem().ShaderResourceTexture;
	VolumetricCloudParams.CloudBilinearTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	// Light0Shadow
/*#if RHI_RAYTRACING
	InitializeBlueNoise(VolumetricCloudParams.BlueNoise);
#else
	// Blue noise texture is undified for some configuration so replace by other noise for now.
	VolumetricCloudParams.BlueNoise.Dimensions = FIntVector(16, 16, 4); // 16 is the size of the tile, so 4 dimension for the 64x64 HighFrequencyNoiseTexture.
	VolumetricCloudParams.BlueNoise.Texture = GEngine->HighFrequencyNoiseTexture->Resource->TextureRHI;
#endif*/
	VolumetricCloudParams.SubSetCoordToFullResolutionScaleBias = FUintVector4(1, 1, 0, 0);
	VolumetricCloudParams.NoiseFrameIndexModPattern = 0;
	VolumetricCloudParams.VolumetricRenderTargetMode = GetVolumetricRenderTargetMode();
	VolumetricCloudParams.SampleCountDebugMode = FMath::Clamp(CVarVolumetricCloudDebugSampleCountMode.GetValueOnAnyThread(), 0, 5);
}

//////////////////////////////////////////////////////////////////////////

class FRenderVolumetricCloudVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRenderVolumetricCloudVS, MeshMaterial);

public:

	FRenderVolumetricCloudVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FRenderVolumetricCloudGlobalParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FRenderVolumetricCloudVS() {}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		const bool bIsCompatible = IsMaterialCompatibleWithVolumetricCloud(Parameters.MaterialParameters, Parameters.Platform);
		return bIsCompatible;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_MAINVS"), TEXT("1"));
	}

private:
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRenderVolumetricCloudVS, TEXT("/Engine/Private/VolumetricCloud.usf"), TEXT("MainVS"), SF_Vertex);

//////////////////////////////////////////////////////////////////////////

enum EVolumetricCloudRenderViewPsPermutations
{
	VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow0,
	VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow0,
	VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow1,
	VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow1,
	VolumetricCloudRenderViewPsCount
};

BEGIN_SHADER_PARAMETER_STRUCT(FRenderVolumetricCloudRenderViewParametersPS, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CloudGroundLightingTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ShadowTexture)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

template<EVolumetricCloudRenderViewPsPermutations Permutation>
class FRenderVolumetricCloudRenderViewPs : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRenderVolumetricCloudRenderViewPs, MeshMaterial);

public:

	FRenderVolumetricCloudRenderViewPs(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FRenderVolumetricCloudGlobalParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FRenderVolumetricCloudRenderViewPs() {}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		const bool bIsCompatible = IsMaterialCompatibleWithVolumetricCloud(Parameters.MaterialParameters, Parameters.Platform);
		return bIsCompatible;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_RENDERVIEW_PS"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("CLOUD_LAYER_PIXEL_SHADER"), TEXT("1"));

		const bool bUseAtmosphereTransmittance = Permutation == VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow0 || Permutation == VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow1;
		OutEnvironment.SetDefine(TEXT("CLOUD_PER_SAMPLE_ATMOSPHERE_TRANSMITTANCE"), bUseAtmosphereTransmittance ? TEXT("1") : TEXT("0"));

		const bool bSampleLightShadowmap = Permutation == VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow1 || Permutation == VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow1;
		OutEnvironment.SetDefine(TEXT("CLOUD_SAMPLE_ATMOSPHERIC_LIGHT_SHADOWMAP"), bSampleLightShadowmap ? TEXT("1") : TEXT("0"));
	}

private:
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow0>, TEXT("/Engine/Private/VolumetricCloud.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow0>, TEXT("/Engine/Private/VolumetricCloud.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow1>, TEXT("/Engine/Private/VolumetricCloud.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow1>, TEXT("/Engine/Private/VolumetricCloud.usf"), TEXT("MainPS"), SF_Pixel);



//////////////////////////////////////////////////////////////////////////

class FSingleTriangleMeshVertexBuffer : public FRenderResource
{
public:
	FStaticMeshVertexBuffers Buffers;

	FSingleTriangleMeshVertexBuffer()
	{
		TArray<FDynamicMeshVertex> Vertices;

		// Vertex position constructed in the shader
		Vertices.Add(FDynamicMeshVertex(FVector(0.0f, 0.0f, 0.0f)));
		Vertices.Add(FDynamicMeshVertex(FVector(0.0f, 0.0f, 0.0f)));
		Vertices.Add(FDynamicMeshVertex(FVector(0.0f, 0.0f, 0.0f)));

		Buffers.PositionVertexBuffer.Init(Vertices.Num());
		Buffers.StaticMeshVertexBuffer.Init(Vertices.Num(), 1);

		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];

			Buffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			Buffers.StaticMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector());
			Buffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
		}
	}

	virtual void InitRHI() override
	{
		Buffers.PositionVertexBuffer.InitResource();
		Buffers.StaticMeshVertexBuffer.InitResource();
	}

	virtual void ReleaseRHI() override
	{
		Buffers.PositionVertexBuffer.ReleaseResource();
		Buffers.StaticMeshVertexBuffer.ReleaseResource();
	}
};

static TGlobalResource<FSingleTriangleMeshVertexBuffer> GSingleTriangleMeshVertexBuffer;

class FSingleTriangleMeshVertexFactory : public FLocalVertexFactory
{
public:
	FSingleTriangleMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FSingleTriangleMeshVertexFactory")
	{}

	~FSingleTriangleMeshVertexFactory()
	{
		ReleaseResource();
	}

	virtual void InitRHI() override
	{
		FSingleTriangleMeshVertexBuffer* VertexBuffer = &GSingleTriangleMeshVertexBuffer;
		FLocalVertexFactory::FDataType NewData;
		VertexBuffer->Buffers.PositionVertexBuffer.BindPositionVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(this, NewData, 0);
		FColorVertexBuffer::BindDefaultColorVertexBuffer(this, NewData, FColorVertexBuffer::NullBindStride::ZeroForDefaultBufferBind);
		// Don't call SetData(), because that ends up calling UpdateRHI(), and if the resource has already been initialized
		// (e.g. when switching the feature level in the editor), that calls InitRHI(), resulting in an infinite loop.
		Data = NewData;
		FLocalVertexFactory::InitRHI();
	}

	bool HasIncompatibleFeatureLevel(ERHIFeatureLevel::Type InFeatureLevel)
	{
		return InFeatureLevel != GetFeatureLevel();
	}
};

static FSingleTriangleMeshVertexFactory* GSingleTriangleMeshVertexFactory = NULL;

static void GetSingleTriangleMeshBatch(FMeshBatch& LocalSingleTriangleMesh, const FMaterialRenderProxy* CloudVolumeMaterialProxy, const ERHIFeatureLevel::Type FeatureLevel)
{
	if (!GSingleTriangleMeshVertexFactory || GSingleTriangleMeshVertexFactory->HasIncompatibleFeatureLevel(FeatureLevel))
	{
		if (GSingleTriangleMeshVertexFactory)
		{
			GSingleTriangleMeshVertexFactory->ReleaseResource();
			delete GSingleTriangleMeshVertexFactory;
		}
		GSingleTriangleMeshVertexFactory = new FSingleTriangleMeshVertexFactory(FeatureLevel);
		GSingleTriangleMeshVertexBuffer.UpdateRHI();
		GSingleTriangleMeshVertexFactory->InitResource();
	}
	LocalSingleTriangleMesh.VertexFactory = GSingleTriangleMeshVertexFactory;
	LocalSingleTriangleMesh.MaterialRenderProxy = CloudVolumeMaterialProxy;
	LocalSingleTriangleMesh.Elements[0].IndexBuffer = nullptr;
	LocalSingleTriangleMesh.Elements[0].FirstIndex = 0;
	LocalSingleTriangleMesh.Elements[0].NumPrimitives = 1;
	LocalSingleTriangleMesh.Elements[0].MinVertexIndex = 0;
	LocalSingleTriangleMesh.Elements[0].MaxVertexIndex = 2;

	LocalSingleTriangleMesh.Elements[0].PrimitiveUniformBuffer = nullptr;
	LocalSingleTriangleMesh.Elements[0].PrimitiveIdMode = PrimID_ForceZero;
}



//////////////////////////////////////////////////////////////////////////



static bool GetSkyUsesPerPixelTransmittance(const FScene* Scene, const FViewInfo* InViewIfDynamicMeshCommand)
{
	return CVarVolumetricCloudPerSampleSkyAtmosphereTransmittance.GetValueOnRenderThread() > 0 &&
		Scene->HasSkyAtmosphere() && ShouldRenderSkyAtmosphere(Scene, InViewIfDynamicMeshCommand->Family->EngineShowFlags);
}

class FVolumetricCloudRenderViewMeshProcessor : public FMeshPassProcessor
{
public:
	FVolumetricCloudRenderViewMeshProcessor(const FScene* Scene, const FViewInfo* InViewIfDynamicMeshCommand, 
		TUniformBufferRef<FViewUniformShaderParameters> ViewUniformBuffer, bool bShouldViewRenderVolumetricRenderTarget, bool bSkipAtmosphericLightShadowmap,
		FMeshPassDrawListContext* InDrawListContext, TUniformBufferRef<FRenderVolumetricCloudGlobalParameters> VolumetricCloudParmsUB)
		: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
		, bVolumetricCloudPerSampleAtmosphereTransmittance(GetSkyUsesPerPixelTransmittance(Scene, InViewIfDynamicMeshCommand))
		, bVolumetricCloudSampleLightShadowmap(!bSkipAtmosphericLightShadowmap && CVarVolumetricCloudShadowSampleAtmosphericLightShadowmap.GetValueOnAnyThread() > 0)
	{
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		PassDrawRenderState.SetPassUniformBuffer(VolumetricCloudParmsUB);

		PassDrawRenderState.SetViewUniformBuffer(ViewUniformBuffer);

		if (bShouldViewRenderVolumetricRenderTarget)
		{ 
			// No blending as we only render clouds in that render target today. Avoids clearing for now.
			PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
		}
		else
		{
			// When volumetric render target is not enabled globally or for some views, e.g. reflection captures.
			PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI());
		}
	}

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		if (Material.GetMaterialDomain() != MD_Volume)
		{
			// Skip in this case. This can happens when the material is compiled and a fallback is provided.
			return;
		}

		const ERasterizerFillMode MeshFillMode = FM_Solid;
		const ERasterizerCullMode MeshCullMode = CM_None;
		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;
		Process(MeshBatch, BatchElementMask, PrimitiveSceneProxy, MaterialRenderProxy, Material, StaticMeshId, MeshFillMode, MeshCullMode);
	}

private:

	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		int32 StaticMeshId,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode)
	{
		FMeshMaterialShaderElementData EmptyShaderElementData;
		EmptyShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

		const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

		if (bVolumetricCloudSampleLightShadowmap)
		{
			if (bVolumetricCloudPerSampleAtmosphereTransmittance)
			{
				TMeshProcessorShaders< FRenderVolumetricCloudVS, FMeshMaterialShader, FMeshMaterialShader,
					FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow1>> PassShaders;
				PassShaders.PixelShader = MaterialResource.GetShader<FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow1>>(VertexFactory->GetType());
				PassShaders.VertexShader = MaterialResource.GetShader<FRenderVolumetricCloudVS>(VertexFactory->GetType());
				const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
				BuildMeshDrawCommands(
					MeshBatch,
					BatchElementMask,
					PrimitiveSceneProxy,
					MaterialRenderProxy,
					MaterialResource,
					PassDrawRenderState,
					PassShaders,
					MeshFillMode,
					MeshCullMode,
					SortKey,
					EMeshPassFeatures::Default,
					EmptyShaderElementData);
			}
			else
			{
				TMeshProcessorShaders< FRenderVolumetricCloudVS, FMeshMaterialShader, FMeshMaterialShader,
					FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow1>> PassShaders;
				PassShaders.PixelShader = MaterialResource.GetShader<FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow1>>(VertexFactory->GetType());
				PassShaders.VertexShader = MaterialResource.GetShader<FRenderVolumetricCloudVS>(VertexFactory->GetType());
				const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
				BuildMeshDrawCommands(
					MeshBatch,
					BatchElementMask,
					PrimitiveSceneProxy,
					MaterialRenderProxy,
					MaterialResource,
					PassDrawRenderState,
					PassShaders,
					MeshFillMode,
					MeshCullMode,
					SortKey,
					EMeshPassFeatures::Default,
					EmptyShaderElementData);
			}
		}
		else
		{
			if (bVolumetricCloudPerSampleAtmosphereTransmittance)
			{
				TMeshProcessorShaders< FRenderVolumetricCloudVS, FMeshMaterialShader, FMeshMaterialShader,
					FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow0>> PassShaders;
				PassShaders.PixelShader = MaterialResource.GetShader<FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance1_SampleShadow0>>(VertexFactory->GetType());
				PassShaders.VertexShader = MaterialResource.GetShader<FRenderVolumetricCloudVS>(VertexFactory->GetType());
				const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
				BuildMeshDrawCommands(
					MeshBatch,
					BatchElementMask,
					PrimitiveSceneProxy,
					MaterialRenderProxy,
					MaterialResource,
					PassDrawRenderState,
					PassShaders,
					MeshFillMode,
					MeshCullMode,
					SortKey,
					EMeshPassFeatures::Default,
					EmptyShaderElementData);
			}
			else
			{
				TMeshProcessorShaders< FRenderVolumetricCloudVS, FMeshMaterialShader, FMeshMaterialShader,
					FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow0>> PassShaders;
				PassShaders.PixelShader = MaterialResource.GetShader<FRenderVolumetricCloudRenderViewPs<VolumetricCloudRenderViewPs_PerSampleAtmosphereTransmittance0_SampleShadow0>>(VertexFactory->GetType());
				PassShaders.VertexShader = MaterialResource.GetShader<FRenderVolumetricCloudVS>(VertexFactory->GetType());
				const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
				BuildMeshDrawCommands(
					MeshBatch,
					BatchElementMask,
					PrimitiveSceneProxy,
					MaterialRenderProxy,
					MaterialResource,
					PassDrawRenderState,
					PassShaders,
					MeshFillMode,
					MeshCullMode,
					SortKey,
					EMeshPassFeatures::Default,
					EmptyShaderElementData);
			}
		}
	}

	FMeshPassProcessorRenderState PassDrawRenderState;
	bool bVolumetricCloudPerSampleAtmosphereTransmittance;
	bool bVolumetricCloudSampleLightShadowmap;
};



//////////////////////////////////////////////////////////////////////////

BEGIN_SHADER_PARAMETER_STRUCT(FVolumetricCloudGroundLightingParametersPS, )
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FVolumetricCloudGroundLightingPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVolumetricCloudGroundLightingPS, MeshMaterial);

public:

	FVolumetricCloudGroundLightingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FRenderVolumetricCloudGlobalParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FVolumetricCloudGroundLightingPS() {}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		const bool bIsCompatible = IsMaterialCompatibleWithVolumetricCloud(Parameters.MaterialParameters, Parameters.Platform);
		return bIsCompatible;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_GROUNDLIGHTING_PS"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("CLOUD_LAYER_PIXEL_SHADER"), TEXT("1"));
	}

private:
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FVolumetricCloudGroundLightingPS, TEXT("/Engine/Private/VolumetricCloud.usf"), TEXT("MainPS"), SF_Pixel);



//////////////////////////////////////////////////////////////////////////



class FVolumetricCloudRenderAOMeshProcessor : public FMeshPassProcessor
{
public:
	FVolumetricCloudRenderAOMeshProcessor(const FScene* Scene, const FViewInfo* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext, TUniformBufferRef<FRenderVolumetricCloudGlobalParameters> VolumetricCloudParmsUB)
		: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	{
		PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
		PassDrawRenderState.SetPassUniformBuffer(VolumetricCloudParmsUB);
	}

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		check(Material.GetMaterialDomain() == MD_Volume);

		const ERasterizerFillMode MeshFillMode = FM_Solid;
		const ERasterizerCullMode MeshCullMode = CM_None;
		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;
		Process(MeshBatch, BatchElementMask, PrimitiveSceneProxy, MaterialRenderProxy, Material, StaticMeshId, MeshFillMode, MeshCullMode);
	}

private:

	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		int32 StaticMeshId,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode)
	{
		FMeshMaterialShaderElementData EmptyShaderElementData;
		EmptyShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

		const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

		TMeshProcessorShaders< FRenderVolumetricCloudVS, FMeshMaterialShader, FMeshMaterialShader,
			FVolumetricCloudGroundLightingPS> PassShaders;
		PassShaders.PixelShader = MaterialResource.GetShader<FVolumetricCloudGroundLightingPS>(VertexFactory->GetType());
		PassShaders.VertexShader = MaterialResource.GetShader<FRenderVolumetricCloudVS>(VertexFactory->GetType());
		const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
		BuildMeshDrawCommands(
			MeshBatch,
			BatchElementMask,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			MaterialResource,
			PassDrawRenderState,
			PassShaders,
			MeshFillMode,
			MeshCullMode,
			SortKey,
			EMeshPassFeatures::Default,
			EmptyShaderElementData);
	}

	FMeshPassProcessorRenderState PassDrawRenderState;
};



//////////////////////////////////////////////////////////////////////////

BEGIN_SHADER_PARAMETER_STRUCT(FVolumetricCloudShadowParametersPS, )
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FVolumetricCloudShadowPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVolumetricCloudShadowPS, MeshMaterial);

public:

	FVolumetricCloudShadowPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FRenderVolumetricCloudGlobalParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FVolumetricCloudShadowPS() {}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		const bool bIsCompatible = IsMaterialCompatibleWithVolumetricCloud(Parameters.MaterialParameters, Parameters.Platform);
		return bIsCompatible;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_SHADOW_PS"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("CLOUD_LAYER_PIXEL_SHADER"), TEXT("1"));
	}

private:
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FVolumetricCloudShadowPS, TEXT("/Engine/Private/VolumetricCloud.usf"), TEXT("MainPS"), SF_Pixel);



class FVolumetricCloudRenderShadowMeshProcessor : public FMeshPassProcessor
{
public:
	FVolumetricCloudRenderShadowMeshProcessor(const FScene* Scene, const FViewInfo* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext, TUniformBufferRef<FRenderVolumetricCloudGlobalParameters> VolumetricCloudParmsUB)
		: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	{
		PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
		PassDrawRenderState.SetPassUniformBuffer(VolumetricCloudParmsUB);
	}

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		check(Material.GetMaterialDomain() == MD_Volume);

		const ERasterizerFillMode MeshFillMode = FM_Solid;
		const ERasterizerCullMode MeshCullMode = CM_None;
		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;
		Process(MeshBatch, BatchElementMask, PrimitiveSceneProxy, MaterialRenderProxy, Material, StaticMeshId, MeshFillMode, MeshCullMode);
	}

private:

	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		int32 StaticMeshId,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode)
	{
		FMeshMaterialShaderElementData EmptyShaderElementData;
		EmptyShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

		const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

		TMeshProcessorShaders< FRenderVolumetricCloudVS, FMeshMaterialShader, FMeshMaterialShader,
			FVolumetricCloudShadowPS> PassShaders;
		PassShaders.PixelShader = MaterialResource.GetShader<FVolumetricCloudShadowPS>(VertexFactory->GetType());
		PassShaders.VertexShader = MaterialResource.GetShader<FRenderVolumetricCloudVS>(VertexFactory->GetType());
		const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
		BuildMeshDrawCommands(
			MeshBatch,
			BatchElementMask,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			MaterialResource,
			PassDrawRenderState,
			PassShaders,
			MeshFillMode,
			MeshCullMode,
			SortKey,
			EMeshPassFeatures::Default,
			EmptyShaderElementData);
	}

	FMeshPassProcessorRenderState PassDrawRenderState;
};



//////////////////////////////////////////////////////////////////////////

class FDrawDebugCloudShadowCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDrawDebugCloudShadowCS);
	SHADER_USE_PARAMETER_STRUCT(FDrawDebugCloudShadowCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FRenderVolumetricCloudGlobalParameters, RenderVolumetricCloudParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderDrawDebug::FShaderDrawDebugParameters, ShaderDrawParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CloudShadowTexture)
		SHADER_PARAMETER(FIntPoint, CloudShadowTextureResolution)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsVolumetricCloudMaterialSupported(Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_DEBUG_SHADOW_CS"), TEXT("1"));
	}
};

IMPLEMENT_GLOBAL_SHADER(FDrawDebugCloudShadowCS, "/Engine/Private/VolumetricCloud.usf", "MainDrawDebugShadowCS", SF_Compute);



//////////////////////////////////////////////////////////////////////////

class FCloudShadowFilterCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCloudShadowFilterCS);
	SHADER_USE_PARAMETER_STRUCT(FCloudShadowFilterCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FRenderVolumetricCloudGlobalParameters, RenderVolumetricCloudParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CloudShadowTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutCloudShadowTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsVolumetricCloudMaterialSupported(Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_SHADOW_FILTER_CS"), TEXT("1"));
	}
};

IMPLEMENT_GLOBAL_SHADER(FCloudShadowFilterCS, "/Engine/Private/VolumetricCloud.usf", "MainShadowFilterCS", SF_Compute);



//////////////////////////////////////////////////////////////////////////


static bool CloudGroundLightingRenderPassEnabled(FVolumetricCloudCommonShaderParameters& CloudGlobalShaderParams)
{
	return false; // Always disable for now. Quality is not great. Keeping the code around however because we might use it to affect the aerial perspective later.
	//CVarVolumetricCloudGroundLightingOverrideExtend.GetValueOnAnyThread() != 0.0f // -1 is no override, >0 is override, ==0 is disabled
	//	&& CVarVolumetricCloudGroundLightingSampleCount.GetValueOnAnyThread() > 0 
	//	&& (CloudGlobalShaderParams.GroundAlbedo != FLinearColor::Black);
}


void FSceneRenderer::InitVolumetricCloudsForViews(FRHICommandListImmediate& RHICmdList)
{
	if (Scene)
	{
		check(ShouldRenderVolumetricCloud(Scene, ViewFamily.EngineShowFlags)); // This should not be called if we should not render SkyAtmosphere

		check(Scene->GetVolumetricCloudSceneInfo());
		const FSkyAtmosphereRenderSceneInfo* SkyInfo = Scene->GetSkyAtmosphereSceneInfo();
		FVolumetricCloudRenderSceneInfo& CloudInfo = *Scene->GetVolumetricCloudSceneInfo();
		const FVolumetricCloudSceneProxy& CloudProxy = CloudInfo.GetVolumetricCloudSceneProxy();
		const float KilometersToCentimeters = 100000.0f;
		const float CentimetersToKilometers = 1.0f / KilometersToCentimeters;

		{
			FVolumetricCloudCommonShaderParameters& CloudGlobalShaderParams = CloudInfo.GetVolumetricCloudCommonShaderParameters();
			float PlanetRadiusKm = CloudProxy.PlanetRadiusKm;
			if (SkyInfo)
			{
				const FAtmosphereSetup& AtmosphereSetup = SkyInfo->GetSkyAtmosphereSceneProxy().GetAtmosphereSetup();
				PlanetRadiusKm = AtmosphereSetup.BottomRadiusKm;
				CloudGlobalShaderParams.CloudLayerCenterKm = AtmosphereSetup.PlanetCenterKm;
			}
			else
			{
				CloudGlobalShaderParams.CloudLayerCenterKm = FVector(0.0f, 0.0f, -PlanetRadiusKm);
			}
			CloudGlobalShaderParams.PlanetRadiusKm = PlanetRadiusKm;
			CloudGlobalShaderParams.BottomRadiusKm = PlanetRadiusKm + CloudProxy.LayerBottomAltitudeKm;
			CloudGlobalShaderParams.TopRadiusKm = CloudGlobalShaderParams.BottomRadiusKm + CloudProxy.LayerHeightKm;
			CloudGlobalShaderParams.GroundAlbedo = FLinearColor(CloudProxy.GroundAlbedo);
			CloudGlobalShaderParams.AtmosphericLightsContributionFactor = CloudProxy.AtmosphericLightsContributionFactor;

			CloudGlobalShaderParams.TracingStartMaxDistance = KilometersToCentimeters * CVarVolumetricCloudTracingStartMaxDistance.GetValueOnAnyThread();
			CloudGlobalShaderParams.TracingMaxDistance = KilometersToCentimeters * CVarVolumetricCloudTracingMaxDistance.GetValueOnAnyThread();

			CloudGlobalShaderParams.SampleCountMax = FMath::Max(2, CVarVolumetricCloudSampleCountMax.GetValueOnAnyThread());
			CloudGlobalShaderParams.InvDistanceToSampleCountMax = 1.0f / FMath::Max(1.0f, KilometersToCentimeters * CVarVolumetricCloudDistanceToSampleCountMax.GetValueOnAnyThread());
			CloudGlobalShaderParams.ShadowSampleCountMax = FMath::Max(2, CVarVolumetricCloudShadowSampleCountMax.GetValueOnAnyThread());
			CloudGlobalShaderParams.ShadowTracingMaxDistance = KilometersToCentimeters * FMath::Max(0.1f, CVarVolumetricCloudShadowTracingMaxDistance.GetValueOnAnyThread());

			// The ambient occlusion texture is always at the top of the planet and parallel to plane with +Z as normal.
			// As such it cannot be used for AO all around a planet, but only for a landscape at the top of a planet.
			// This is for several reasons: 
			//	- It makes the texel content stable under camera translation (simply use snapping).
			//	- It can be used to project simple AO from clouds themselves on the ground.
			const float GroundLightingOverrideExtend = KilometersToCentimeters * CVarVolumetricCloudGroundLightingOverrideExtend.GetValueOnAnyThread();
			const float AOFarDistance = GroundLightingOverrideExtend > 0.0f ? GroundLightingOverrideExtend : CloudGlobalShaderParams.TracingStartMaxDistance + CloudGlobalShaderParams.TracingMaxDistance;
			CloudGlobalShaderParams.GroundLightingTextureWorldCenter = (CloudGlobalShaderParams.CloudLayerCenterKm + FVector(0.0f, 0.0f, PlanetRadiusKm)) * KilometersToCentimeters;
			if (CloudGroundLightingRenderPassEnabled(CloudGlobalShaderParams))
			{
				CloudGlobalShaderParams.GroundLightingUVector = FVector(AOFarDistance, 0.0f, 0.0f);
				CloudGlobalShaderParams.GroundLightingVVector = FVector(0.0f, AOFarDistance, 0.0f);
				CloudGlobalShaderParams.GroundLightingUVectorInv = FVector(1.0f / AOFarDistance, 0.0f, 0.0f);
				CloudGlobalShaderParams.GroundLightingVVectorInv = FVector(0.0f, 1.0f / AOFarDistance, 0.0f);
				CloudGlobalShaderParams.GroundLightingSampleCount = FMath::Max(1, CVarVolumetricCloudGroundLightingSampleCount.GetValueOnAnyThread());
			}
			else
			{
				CloudGlobalShaderParams.GroundLightingUVector = FVector(0.0f, 0.0f, 0.0f);
				CloudGlobalShaderParams.GroundLightingVVector = FVector(0.0f, 0.0f, 0.0f);
				CloudGlobalShaderParams.GroundLightingUVectorInv = FVector(0.0f, 0.0f, 0.0f);
				CloudGlobalShaderParams.GroundLightingVVectorInv = FVector(0.0f, 0.0f, 0.0f);
				CloudGlobalShaderParams.GroundLightingSampleCount = 0.0f;
			}

			CloudGlobalShaderParams.EnableAerialPerspectiveSampling = CVarVolumetricCloudEnableAerialPerspectiveSampling.GetValueOnAnyThread() > 0;
			CloudGlobalShaderParams.EnableDistantSkyLightSampling = CVarVolumetricCloudEnableDistantSkyLightSampling.GetValueOnAnyThread() > 0;
			CloudGlobalShaderParams.EnableAtmosphericLightsSampling = CVarVolumetricCloudEnableAtmosphericLightsSampling.GetValueOnAnyThread() > 0;

			const float CloudShadowmapResolution = float(GetVolumetricCloudShadowMapResolution());
			const float CloudShadowmapResolutionInv = 1.0f / CloudShadowmapResolution;
			CloudGlobalShaderParams.ShadowTextureSizeInvSize = FVector4(CloudShadowmapResolution, CloudShadowmapResolution, CloudShadowmapResolutionInv, CloudShadowmapResolutionInv);
			CloudGlobalShaderParams.ShadowmapStrength = FMath::Max(0.0f, CVarVolumetricCloudShadowMapStrength.GetValueOnAnyThread());

			FLightSceneInfo* LightSceneInfo = Scene->AtmosphereLights[0];
			if(LightSceneInfo)
			{
				const FVector AtmopshericLight0Direction = LightSceneInfo->Proxy->GetDirection();
				const FVector UpVector = FMath::Abs(FVector::DotProduct(AtmopshericLight0Direction, FVector::UpVector)) > 0.99f ? FVector::ForwardVector : FVector::UpVector;
				const float OverrideExtent = CVarVolumetricCloudShadowMapOverrideExtend.GetValueOnAnyThread() * KilometersToCentimeters;

				const float SphereRadius = OverrideExtent > 0.0f ? OverrideExtent : CloudGlobalShaderParams.TracingStartMaxDistance + CloudGlobalShaderParams.TracingMaxDistance;
				const float NearPlane = 0.0f;
				const float FarPlane = 2.0f * SphereRadius;
				const float ZScale = 1.0f / (FarPlane - NearPlane);
				const float ZOffset = -NearPlane;

				// TODO Make it waork for all views
				FVector LookAtPosition = FVector::ZeroVector;
				FVector PlanetToCameraNormUp = FVector::UpVector;
				if (Views.Num() > 0)
				{
					FViewInfo& View = Views[0];
					LookAtPosition = (View.ViewMatrices.GetViewOrigin() - (CloudGlobalShaderParams.CloudLayerCenterKm * KilometersToCentimeters));
					LookAtPosition.Normalize();
					PlanetToCameraNormUp = LookAtPosition;
					LookAtPosition = (CloudGlobalShaderParams.CloudLayerCenterKm + LookAtPosition * PlanetRadiusKm) * KilometersToCentimeters;
					const FVector LightPosition = LookAtPosition - AtmopshericLight0Direction * SphereRadius;

					FLookAtMatrix Temp(LightPosition, LookAtPosition, UpVector);

					FVector X, Y, Z;
					Temp.GetUnitAxes(X, Y, Z);
					float PosX = FVector::DotProduct(X, LookAtPosition);
					float PosY = FVector::DotProduct(Y, LookAtPosition);
					float PosZ = FVector::DotProduct(Z, LookAtPosition);
					float TexelWorldSize = CloudShadowmapResolutionInv * SphereRadius * 2.0f;
					PosX = FMath::FloorToFloat(PosX / TexelWorldSize) * TexelWorldSize;
					PosY = FMath::FloorToFloat(PosY / TexelWorldSize) * TexelWorldSize;
					//PosZ = FMath::FloorToFloat(PosZ / TexelWorldSize) * TexelWorldSize; // This is not required but still doing it because mapping LightPosition on the virtual planet will introduce flickering
					LookAtPosition = X * PosX + Y * PosY + Z * PosZ;
				}

				const FVector LightPosition = LookAtPosition - AtmopshericLight0Direction * SphereRadius;
				FReversedZOrthoMatrix ShadowProjectionMatrix(SphereRadius, SphereRadius, ZScale, ZOffset);
				FLookAtMatrix ShadowViewMatrix(LightPosition, LookAtPosition, UpVector);
				CloudGlobalShaderParams.WorldToLightClipShadowMatrix = ShadowViewMatrix * ShadowProjectionMatrix;
				CloudGlobalShaderParams.WorldToLightClipShadowMatrixInv = CloudGlobalShaderParams.WorldToLightClipShadowMatrix.InverseFast();
				CloudGlobalShaderParams.Light0ShadowDirection = AtmopshericLight0Direction;
				CloudGlobalShaderParams.ShadowmapFarDepthKm = FarPlane * CentimetersToKilometers;

				// More samples when the sun is at the horizon: a lot more distance to travel and less pixel covered so trying to keep the same cost and quality.
				CloudGlobalShaderParams.LightShadowmapSampleClount = 16.0f + 32.0f * FMath::Clamp(0.2f / FMath::Abs(FVector::DotProduct(PlanetToCameraNormUp, AtmopshericLight0Direction)) - 1.0f, 0.0f, 1.0f);
			}
			else
			{
				CloudGlobalShaderParams.WorldToLightClipShadowMatrix = FMatrix::Identity;
				CloudGlobalShaderParams.WorldToLightClipShadowMatrixInv = FMatrix::Identity;
				CloudGlobalShaderParams.ShadowmapFarDepthKm = 1.0f;
				CloudGlobalShaderParams.LightShadowmapSampleClount = 0.0f;
			}

			FVolumetricCloudCommonGlobalShaderParameters CloudGlobalShaderParamsUB;
			CloudGlobalShaderParamsUB.VolumetricCloudCommonParams = CloudGlobalShaderParams;
			CloudInfo.GetVolumetricCloudCommonShaderParametersUB() = TUniformBufferRef<FVolumetricCloudCommonGlobalShaderParameters>::CreateUniformBufferImmediate(CloudGlobalShaderParamsUB, UniformBuffer_SingleFrame);
		}



		if (CloudProxy.GetCloudVolumeMaterial())
		{
			FMaterialRenderProxy* CloudVolumeMaterialProxy = CloudProxy.GetCloudVolumeMaterial()->GetRenderProxy();
			if (CloudVolumeMaterialProxy->GetMaterial(ViewFamily.GetFeatureLevel())->GetMaterialDomain() == MD_Volume)
			{
				SCOPED_DRAW_EVENT(RHICmdList, VolumetricCloudShadow);
				SCOPED_GPU_STAT(RHICmdList, VolumetricCloudShadow);

				FRDGBuilder GraphBuilder(RHICmdList);

				FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
				TRefCountPtr<IPooledRenderTarget> BlackDummy = GSystemTextures.BlackDummy;
				FRDGTextureRef BlackDummyRDG = GraphBuilder.RegisterExternalTexture(BlackDummy);

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					FViewInfo& ViewInfo = Views[ViewIndex];
					FVector ViewOrigin = ViewInfo.ViewMatrices.GetViewOrigin();

					FVolumeShadowingShaderParametersGlobal0 LightShadowShaderParams0;
					SetVolumeShadowingDefaultShaderParameters(LightShadowShaderParams0);

					FRenderVolumetricCloudGlobalParameters VolumetricCloudParams;
					VolumetricCloudParams.Light0Shadow = LightShadowShaderParams0;
					SetupDefaultRenderVolumetricCloudGlobalParameters(VolumetricCloudParams, CloudInfo);
					TUniformBufferRef<FRenderVolumetricCloudGlobalParameters> VolumetricCloudParamsUB = TUniformBufferRef<FRenderVolumetricCloudGlobalParameters>::CreateUniformBufferImmediate(VolumetricCloudParams, UniformBuffer_SingleFrame);



					// Render AO

					//FRDGTextureRef CloudGroundLightingTexture = BlackDummyRDG;
					if (CloudGroundLightingRenderPassEnabled(VolumetricCloudParams.VolumetricCloud))
					{
						check(false);// This needs to be redone (resource on view if view dependent, or on SceneInfo if not)

						/*int32 GroundLightingTextureResolution = FMath::Max(256, CVarVolumetricCloudGroundLightingTextureResolution.GetValueOnAnyThread());
						CloudGroundLightingTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2DDesc(FIntPoint(GroundLightingTextureResolution, GroundLightingTextureResolution),
							PF_FloatRGB, FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable, false, 1), TEXT("CloudGroundLightingTexture"));

						FVolumetricCloudGroundLightingParametersPS* CloudGroundLightingParameters = GraphBuilder.AllocParameters<FVolumetricCloudGroundLightingParametersPS>();
						CloudGroundLightingParameters->RenderTargets[0] = FRenderTargetBinding(CloudGroundLightingTexture, ERenderTargetLoadAction::ENoAction);

						GraphBuilder.AddPass(
							RDG_EVENT_NAME("CloudGroundLighting"),
							CloudGroundLightingParameters,
							ERDGPassFlags::Raster,
							[CloudGroundLightingParameters, Scene = Scene, &ViewInfo, &CloudVolumeMaterialProxy, &CloudInfo, SceneDepthZ, BlackDummy, VolumetricCloudParamsUB](FRHICommandListImmediate& RHICmdList)
						{
							DrawDynamicMeshPass(ViewInfo, RHICmdList,
								[&ViewInfo, &CloudVolumeMaterialProxy, &RHICmdList, &VolumetricCloudParamsUB](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
								{
									FVolumetricCloudRenderAOMeshProcessor PassMeshProcessor(
										ViewInfo.Family->Scene->GetRenderScene(), &ViewInfo,
										DynamicMeshPassContext, VolumetricCloudParamsUB);

									FMeshBatch LocalSingleTriangleMesh;
									GetSingleTriangleMeshBatch(LocalSingleTriangleMesh, CloudVolumeMaterialProxy, ViewInfo);

									const FPrimitiveSceneProxy* PrimitiveSceneProxy = nullptr;
									const uint64 DefaultBatchElementMask = ~0ull;
									PassMeshProcessor.AddMeshBatch(LocalSingleTriangleMesh, DefaultBatchElementMask, PrimitiveSceneProxy);
								});
						});*/
					}



					// Render atmospheric lights shadow maps
					if (CVarVolumetricCloudShadowMap.GetValueOnAnyThread() > 0)
					{
						const uint32 VolumetricCloudShadowMapResolution = GetVolumetricCloudShadowMapResolution();
						FRDGTextureRef CloudShadowTexture = GraphBuilder.CreateTexture(
							FRDGTextureDesc::Create2DDesc(FIntPoint(VolumetricCloudShadowMapResolution, VolumetricCloudShadowMapResolution), PF_G16R16F,
								FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable, false, 1), TEXT("CloudShadowTexture"));

						FVolumetricCloudShadowParametersPS* CloudShadowParameters = GraphBuilder.AllocParameters<FVolumetricCloudShadowParametersPS>();
						CloudShadowParameters->RenderTargets[0] = FRenderTargetBinding(CloudShadowTexture, ERenderTargetLoadAction::ENoAction);

						GraphBuilder.AddPass(
							RDG_EVENT_NAME("CloudShadow"),
							CloudShadowParameters,
							ERDGPassFlags::Raster,
							[CloudShadowParameters, Scene = Scene, &ViewInfo, &CloudVolumeMaterialProxy, VolumetricCloudParamsUB](FRHICommandListImmediate& RHICmdList)
						{
							DrawDynamicMeshPass(ViewInfo, RHICmdList,
								[&ViewInfo, &CloudVolumeMaterialProxy, &RHICmdList, &VolumetricCloudParamsUB](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
							{
								FVolumetricCloudRenderShadowMeshProcessor PassMeshProcessor(
									ViewInfo.Family->Scene->GetRenderScene(), &ViewInfo,
									DynamicMeshPassContext, VolumetricCloudParamsUB);

								FMeshBatch LocalSingleTriangleMesh;
								GetSingleTriangleMeshBatch(LocalSingleTriangleMesh, CloudVolumeMaterialProxy, ViewInfo.GetFeatureLevel());

								const FPrimitiveSceneProxy* PrimitiveSceneProxy = nullptr;
								const uint64 DefaultBatchElementMask = ~0ull;
								PassMeshProcessor.AddMeshBatch(LocalSingleTriangleMesh, DefaultBatchElementMask, PrimitiveSceneProxy);
							});
						});



						if(CVarVolumetricCloudShadowFiltering.GetValueOnAnyThread() > 0)
						{
							FRDGTextureRef CloudShadowTexture2 = GraphBuilder.CreateTexture(
								FRDGTextureDesc::Create2DDesc(FIntPoint(VolumetricCloudShadowMapResolution, VolumetricCloudShadowMapResolution), PF_G16R16F,
									FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false, 1), TEXT("CloudShadowTexture2"));

							FCloudShadowFilterCS::FPermutationDomain Permutation;
							TShaderMapRef<FCloudShadowFilterCS> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5), Permutation);

							FCloudShadowFilterCS::FParameters* Parameters = GraphBuilder.AllocParameters<FCloudShadowFilterCS::FParameters>();
							Parameters->RenderVolumetricCloudParameters = VolumetricCloudParamsUB;
							Parameters->ViewUniformBuffer = ViewInfo.ViewUniformBuffer;
							Parameters->CloudShadowTexture = CloudShadowTexture;
							Parameters->OutCloudShadowTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(CloudShadowTexture2));

							const FIntVector CloudShadowTextureSize = CloudShadowTexture->Desc.GetSize();
							const FIntVector DispatchCount = DispatchCount.DivideAndRoundUp(FIntVector(CloudShadowTextureSize.X, CloudShadowTextureSize.Y, 1), FIntVector(8, 8, 1));
							FComputeShaderUtils::AddPass(
								GraphBuilder, RDG_EVENT_NAME("FilterCloudShadow"),
								ComputeShader, Parameters, DispatchCount);

							CloudShadowTexture = CloudShadowTexture2;
						}



						GraphBuilder.QueueTextureExtraction(CloudShadowTexture, &ViewInfo.VolumetricCloudShadowMap);
					}
				}

				GraphBuilder.Execute();
			}
		}
	}
}

CloudRenderContext::CloudRenderContext()
{
	SubSetCoordToFullResolutionScaleBias = FUintVector4(1, 1, 0, 0);
	NoiseFrameIndexModPattern = 0;

	bIsReflectionRendering = false;
	bSkipAtmosphericLightShadowmap = false;
}

void FSceneRenderer::RenderVolumetricCloudsInternal(FRDGBuilder& GraphBuilder, CloudRenderContext& CloudRC)
{
	FRenderVolumetricCloudRenderViewParametersPS* RenderViewPassParameters = GraphBuilder.AllocParameters<FRenderVolumetricCloudRenderViewParametersPS>();
	RenderViewPassParameters->RenderTargets = CloudRC.RenderTargets;
	RenderViewPassParameters->ShadowTexture = CloudRC.VolumetricCloudShadowTexture;
	RenderViewPassParameters->CloudGroundLightingTexture = RenderViewPassParameters->ShadowTexture;

	// Copy parameters to lambda
	check(CloudRC.MainView);
	check(CloudRC.CloudInfo);
	check(CloudRC.CloudVolumeMaterialProxy);
	FViewInfo& MainView = *CloudRC.MainView;
	FVolumetricCloudRenderSceneInfo& CloudInfo = *CloudRC.CloudInfo;
	FMaterialRenderProxy* CloudVolumeMaterialProxy = CloudRC.CloudVolumeMaterialProxy;
	TUniformBufferRef<FViewUniformShaderParameters> ViewUniformBuffer = CloudRC.ViewUniformBuffer;
	const bool bShouldViewRenderVolumetricRenderTarget = CloudRC.bShouldViewRenderVolumetricRenderTarget;
	const bool bIsReflectionRendering = CloudRC.bIsReflectionRendering;
	const bool bSkipAtmosphericLightShadowmap = CloudRC.bSkipAtmosphericLightShadowmap;

	FUintVector4 SubSetCoordToFullResolutionScaleBias = CloudRC.SubSetCoordToFullResolutionScaleBias;
	uint32 NoiseFrameIndexModPattern = CloudRC.NoiseFrameIndexModPattern;
	TRefCountPtr<IPooledRenderTarget> SceneDepthZ = CloudRC.SceneDepthZ;
	FVolumeShadowingShaderParametersGlobal0 LightShadowShaderParams0 = CloudRC.LightShadowShaderParams0;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("CloudView"),
		RenderViewPassParameters,
		ERDGPassFlags::Raster,
		[RenderViewPassParameters, Scene = Scene, &MainView, ViewUniformBuffer, 
		bShouldViewRenderVolumetricRenderTarget, CloudVolumeMaterialProxy, bIsReflectionRendering, bSkipAtmosphericLightShadowmap,
		&CloudInfo, SceneDepthZ, LightShadowShaderParams0, SubSetCoordToFullResolutionScaleBias, NoiseFrameIndexModPattern](FRHICommandListImmediate& RHICmdList)
		{
			FRenderVolumetricCloudGlobalParameters VolumetricCloudParams;
			SetupDefaultRenderVolumetricCloudGlobalParameters(VolumetricCloudParams, CloudInfo);
			VolumetricCloudParams.SceneDepthTexture = SceneDepthZ->GetRenderTargetItem().ShaderResourceTexture;
			VolumetricCloudParams.Light0Shadow = LightShadowShaderParams0;
			VolumetricCloudParams.CloudGroundLightingTexture = RenderViewPassParameters->CloudGroundLightingTexture->GetPooledRenderTarget()->GetRenderTargetItem().ShaderResourceTexture;
			VolumetricCloudParams.ShadowTexture = RenderViewPassParameters->ShadowTexture->GetPooledRenderTarget()->GetRenderTargetItem().ShaderResourceTexture;
			VolumetricCloudParams.SubSetCoordToFullResolutionScaleBias = SubSetCoordToFullResolutionScaleBias;
			VolumetricCloudParams.NoiseFrameIndexModPattern = NoiseFrameIndexModPattern;
			VolumetricCloudParams.IsReflectionRendering = bIsReflectionRendering ? 1 : 0;
			if (bIsReflectionRendering)
			{
				VolumetricCloudParams.VolumetricCloud.SampleCountMax = FMath::Max(2, CVarVolumetricCloudReflectionSampleCountMax.GetValueOnAnyThread());
				VolumetricCloudParams.VolumetricCloud.ShadowSampleCountMax = FMath::Max(2, CVarVolumetricCloudReflectionShadowSampleCountMax.GetValueOnAnyThread());
			}
			TUniformBufferRef<FRenderVolumetricCloudGlobalParameters> VolumetricCloudRenderViewParamsUB = TUniformBufferRef<FRenderVolumetricCloudGlobalParameters>::CreateUniformBufferImmediate(VolumetricCloudParams, UniformBuffer_SingleFrame);

			DrawDynamicMeshPass(MainView, RHICmdList,
				[&MainView, ViewUniformBuffer, bShouldViewRenderVolumetricRenderTarget, bSkipAtmosphericLightShadowmap,
				CloudVolumeMaterialProxy, &RHICmdList, &VolumetricCloudRenderViewParamsUB](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
				{
					FVolumetricCloudRenderViewMeshProcessor PassMeshProcessor(
						MainView.Family->Scene->GetRenderScene(), &MainView, ViewUniformBuffer, bShouldViewRenderVolumetricRenderTarget,
						bSkipAtmosphericLightShadowmap, DynamicMeshPassContext, VolumetricCloudRenderViewParamsUB);

					FMeshBatch LocalSingleTriangleMesh;
					GetSingleTriangleMeshBatch(LocalSingleTriangleMesh, CloudVolumeMaterialProxy, MainView.GetFeatureLevel());

					const FPrimitiveSceneProxy* PrimitiveSceneProxy = nullptr;
					const uint64 DefaultBatchElementMask = ~0ull;
					PassMeshProcessor.AddMeshBatch(LocalSingleTriangleMesh, DefaultBatchElementMask, PrimitiveSceneProxy);
				});
		});
}

void FSceneRenderer::RenderVolumetricCloud(FRHICommandListImmediate& RHICmdList)
{
	check(ShouldRenderVolumetricCloud(Scene, ViewFamily.EngineShowFlags)); // This should not be called if we should not render SkyAtmosphere

	FVolumetricCloudRenderSceneInfo& CloudInfo = *Scene->GetVolumetricCloudSceneInfo();
	FVolumetricCloudSceneProxy& CloudSceneProxy = CloudInfo.GetVolumetricCloudSceneProxy();

	if (CloudSceneProxy.GetCloudVolumeMaterial())
	{
		FMaterialRenderProxy* CloudVolumeMaterialProxy = CloudSceneProxy.GetCloudVolumeMaterial()->GetRenderProxy();
		if (CloudVolumeMaterialProxy->GetMaterial(ViewFamily.GetFeatureLevel())->GetMaterialDomain() == MD_Volume)
		{
			SCOPED_DRAW_EVENT(RHICmdList, VolumetricCloud);
			SCOPED_GPU_STAT(RHICmdList, VolumetricCloud);

			FRDGBuilder GraphBuilder(RHICmdList);

			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> SceneDepthZ = SceneContext.SceneDepthZ;
			TRefCountPtr<IPooledRenderTarget> BlackDummy = GSystemTextures.BlackDummy;
			FRDGTextureRef BlackDummyRDG = GraphBuilder.RegisterExternalTexture(BlackDummy);

			CloudRenderContext CloudRC;
			CloudRC.CloudInfo = &CloudInfo;
			CloudRC.CloudVolumeMaterialProxy= CloudVolumeMaterialProxy;
			CloudRC.SceneDepthZ = SceneDepthZ;

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				FViewInfo& ViewInfo = Views[ViewIndex];

				CloudRC.MainView = &ViewInfo;

				bool bShouldViewRenderVolumetricRenderTarget = ShouldViewRenderVolumetricRenderTarget(ViewInfo);
				CloudRC.bShouldViewRenderVolumetricRenderTarget = bShouldViewRenderVolumetricRenderTarget;
				CloudRC.ViewUniformBuffer = bShouldViewRenderVolumetricRenderTarget ? ViewInfo.VolumetricRenderTargetViewUniformBuffer : ViewInfo.ViewUniformBuffer;

				FRDGTextureRef DestinationRT;
				FRDGTextureRef DestinationRTDepth;
				const bool bUseVolumetricRenderTarget = ShouldViewRenderVolumetricRenderTarget(ViewInfo); // not used by reflection captures for instance
				CloudRC.SubSetCoordToFullResolutionScaleBias = FUintVector4(1, 1, 0, 0);
				CloudRC.NoiseFrameIndexModPattern = ViewInfo.CachedViewUniformShaderParameters->StateFrameIndexMod8;
				if (bUseVolumetricRenderTarget)
				{
					FVolumetricRenderTargetViewStateData& VRT = ViewInfo.ViewState->VolumetricRenderTarget;
					DestinationRT = VRT.GetOrCreateVolumetricTracingRT(GraphBuilder);
					DestinationRTDepth = VRT.GetOrCreateVolumetricTracingRTDepth(GraphBuilder);

					// No action because we only need to render volumetric clouds so we do not blend in that render target.
					// When we have more elements rendered in that target later, we can clear it to default and blend.
					CloudRC.RenderTargets[0] = FRenderTargetBinding(DestinationRT, ERenderTargetLoadAction::ENoAction);
					CloudRC.RenderTargets[1] = FRenderTargetBinding(DestinationRTDepth, ERenderTargetLoadAction::ENoAction);
					CloudRC.SubSetCoordToFullResolutionScaleBias = VRT.GetTracingToFullResResolutionScaleBias();
					CloudRC.NoiseFrameIndexModPattern = VRT.GetNoiseFrameIndexModPattern();
				}
				else
				{
					DestinationRT = GraphBuilder.RegisterExternalTexture(SceneContext.GetSceneColor(), TEXT("SceneColor"));
					FIntVector RtSize = SceneContext.GetSceneColor()->GetDesc().GetSize();
					DestinationRTDepth = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2DDesc(FIntPoint(RtSize.X, RtSize.Y), PF_R16F, FClearValueBinding::Black,
						TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable, false, 1), TEXT("DummyDepth"));
					CloudRC.RenderTargets[0] = FRenderTargetBinding(DestinationRT, ERenderTargetLoadAction::ELoad);
					CloudRC.RenderTargets[1] = FRenderTargetBinding(DestinationRTDepth, ERenderTargetLoadAction::ENoAction);
				}



				const bool bShouldSampleOpaqueShadow = true;
				const FLightSceneInfo* LightVolumetricShadowSceneinfo0 = Scene->AtmosphereLights[0];
				const FProjectedShadowInfo* ProjectedShadowInfo0 = nullptr;
				if (LightVolumetricShadowSceneinfo0)
				{
					ProjectedShadowInfo0 = GetLastCascadeShadowInfo(LightVolumetricShadowSceneinfo0->Proxy, VisibleLightInfos[LightVolumetricShadowSceneinfo0->Id]);
				}
				if (bShouldSampleOpaqueShadow && LightVolumetricShadowSceneinfo0 && ProjectedShadowInfo0)
				{
					SetVolumeShadowingShaderParameters(CloudRC.LightShadowShaderParams0, ViewInfo, LightVolumetricShadowSceneinfo0, ProjectedShadowInfo0, INDEX_NONE);
				}
				else
				{
					SetVolumeShadowingDefaultShaderParameters(CloudRC.LightShadowShaderParams0);
				}
				// Cannot nest a global buffer into another one and we are limited to only one PassUniformBuffer on PassDrawRenderState.
				//TUniformBufferRef<FVolumeShadowingShaderParametersGlobal0> LightShadowShaderParams0UniformBuffer = TUniformBufferRef<FVolumeShadowingShaderParametersGlobal0>::CreateUniformBufferImmediate(LightShadowShaderParams0, UniformBuffer_SingleFrame);

				CloudRC.VolumetricCloudShadowTexture = ViewInfo.VolumetricCloudShadowMap.IsValid() ? GraphBuilder.RegisterExternalTexture(ViewInfo.VolumetricCloudShadowMap) : BlackDummyRDG;

				RenderVolumetricCloudsInternal(GraphBuilder, CloudRC);

				if (bUseVolumetricRenderTarget)
				{
					ViewInfo.ViewState->VolumetricRenderTarget.ExtractToVolumetricTracingRT(GraphBuilder, DestinationRT);
					ViewInfo.ViewState->VolumetricRenderTarget.ExtractToVolumetricTracingRTDepth(GraphBuilder, DestinationRTDepth);
				}



				if (CVarVolumetricCloudShadowMapDebug.GetValueOnAnyThread())
				{
					FRenderVolumetricCloudGlobalParameters VolumetricCloudParams;
					SetupDefaultRenderVolumetricCloudGlobalParameters(VolumetricCloudParams, CloudInfo);
					VolumetricCloudParams.VolumetricCloud = CloudInfo.GetVolumetricCloudCommonShaderParameters();
					VolumetricCloudParams.SceneDepthTexture = SceneDepthZ->GetRenderTargetItem().ShaderResourceTexture;
					VolumetricCloudParams.Light0Shadow = CloudRC.LightShadowShaderParams0;
					VolumetricCloudParams.SubSetCoordToFullResolutionScaleBias = CloudRC.SubSetCoordToFullResolutionScaleBias;
					VolumetricCloudParams.NoiseFrameIndexModPattern = CloudRC.NoiseFrameIndexModPattern;
					TUniformBufferRef<FRenderVolumetricCloudGlobalParameters> VolumetricCloudParamsUB = TUniformBufferRef<FRenderVolumetricCloudGlobalParameters>::CreateUniformBufferImmediate(VolumetricCloudParams, UniformBuffer_SingleFrame);

					FViewElementPDI ShadowFrustumPDI(&ViewInfo, nullptr, nullptr);
					DrawFrustumWireframe(&ShadowFrustumPDI, VolumetricCloudParams.VolumetricCloud.WorldToLightClipShadowMatrixInv, FColor::Purple, 0);

					{
						const float ViewPortWidth = float(ViewInfo.ViewRect.Width());
						const float ViewPortHeight = float(ViewInfo.ViewRect.Height());
						FRenderTargetTemp TempRenderTarget(ViewInfo, (const FTexture2DRHIRef&)SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture);
						FCanvas Canvas(&TempRenderTarget, NULL, ViewInfo.Family->CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, ViewInfo.GetFeatureLevel());
						FLinearColor TextColor(1.0f, 0.5f, 0.0f);
						FString Text = FString::Printf(TEXT("Shadow Sample Count = %.1f"), VolumetricCloudParams.VolumetricCloud.LightShadowmapSampleClount);
						Canvas.DrawShadowedString(ViewPortWidth * 0.5f - Text.Len() * 7.0f, ViewPortHeight * 0.4f, *Text, GetStatsFont(), TextColor);
						Canvas.Flush_RenderThread(RHICmdList);
					}

					if (ShaderDrawDebug::IsShaderDrawDebugEnabled(ViewInfo))
					{
						FDrawDebugCloudShadowCS::FPermutationDomain Permutation;
						TShaderMapRef<FDrawDebugCloudShadowCS> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5), Permutation);

						FDrawDebugCloudShadowCS::FParameters* Parameters = GraphBuilder.AllocParameters<FDrawDebugCloudShadowCS::FParameters>();
						Parameters->RenderVolumetricCloudParameters = VolumetricCloudParamsUB;
						Parameters->ViewUniformBuffer = ViewInfo.ViewUniformBuffer;
						Parameters->CloudShadowTexture = CloudRC.VolumetricCloudShadowTexture;
						ShaderDrawDebug::SetParameters(GraphBuilder, ViewInfo.ShaderDrawData, Parameters->ShaderDrawParameters);

						const FIntVector CloudShadowTextureSize = CloudRC.VolumetricCloudShadowTexture->Desc.GetSize();
						const FIntVector DispatchCount = DispatchCount.DivideAndRoundUp(FIntVector(CloudShadowTextureSize.X, CloudShadowTextureSize.Y, 1), FIntVector(8, 8, 1));
						FComputeShaderUtils::AddPass(
							GraphBuilder, RDG_EVENT_NAME("DrawDebugCloudShadow"),
							ComputeShader, Parameters, DispatchCount);
					}
				}
			}

			GraphBuilder.Execute();
		}
	}

}


