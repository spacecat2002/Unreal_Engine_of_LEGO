// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessing.cpp: The center for all post processing activities.
=============================================================================*/

#include "PostProcess/PostProcessing.h"
#include "EngineGlobals.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "PostProcess/PostProcessInput.h"
#include "PostProcess/PostProcessAA.h"
#if WITH_EDITOR
	#include "PostProcess/PostProcessBufferInspector.h"
#endif
#include "PostProcess/DiaphragmDOF.h"
#include "PostProcess/PostProcessMaterial.h"
#include "PostProcess/PostProcessWeightedSampleSum.h"
#include "PostProcess/PostProcessBloomSetup.h"
#include "PostProcess/PostProcessMobile.h"
#include "PostProcess/PostProcessDownsample.h"
#include "PostProcess/PostProcessHistogram.h"
#include "PostProcess/PostProcessVisualizeHDR.h"
#include "PostProcess/VisualizeShadingModels.h"
#include "PostProcess/PostProcessSelectionOutline.h"
#include "PostProcess/PostProcessGBufferHints.h"
#include "PostProcess/PostProcessVisualizeBuffer.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "PostProcess/PostProcessTonemap.h"
#include "PostProcess/PostProcessLensFlares.h"
#include "PostProcess/PostProcessBokehDOF.h"
#include "PostProcess/PostProcessCombineLUTs.h"
#include "PostProcess/PostProcessTemporalAA.h"
#include "PostProcess/PostProcessMotionBlur.h"
#include "PostProcess/PostProcessDOF.h"
#include "PostProcess/PostProcessUpscale.h"
#include "PostProcess/PostProcessHMD.h"
#include "PostProcess/PostProcessVisualizeComplexity.h"
#include "PostProcess/PostProcessCompositeEditorPrimitives.h"
#include "PostProcess/PostProcessTestImage.h"
#include "PostProcess/PostProcessFFTBloom.h"
#include "PostProcess/PostProcessStreamingAccuracyLegend.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "PostProcess/PostProcessMorpheus.h"
#include "CompositionLighting/PostProcessPassThrough.h"
#include "CompositionLighting/PostProcessLpvIndirect.h"
#include "ShaderPrint.h"
#include "HighResScreenshot.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "BufferVisualizationData.h"
#include "DeferredShadingRenderer.h"
#include "MobileSeparateTranslucencyPass.h"
#include "MobileDistortionPass.h"
#include "SceneTextureParameters.h"
#include "PixelShaderUtils.h"

/** The global center for all post processing activities. */
FPostProcessing GPostProcessing;

static TAutoConsoleVariable<float> CVarDepthOfFieldNearBlurSizeThreshold(
	TEXT("r.DepthOfField.NearBlurSizeThreshold"),
	0.01f,
	TEXT("Sets the minimum near blur size before the effect is forcably disabled. Currently only affects Gaussian DOF.\n")
	TEXT(" (default: 0.01)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDepthOfFieldMaxSize(
	TEXT("r.DepthOfField.MaxSize"),
	100.0f,
	TEXT("Allows to clamp the gaussian depth of field radius (for better performance), default: 100"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRenderTargetSwitchWorkaround(
	TEXT("r.RenderTargetSwitchWorkaround"),
	0,
	TEXT("Workaround needed on some mobile platforms to avoid a performance drop related to switching render targets.\n")
	TEXT("Only enabled on some hardware. This affects the bloom quality a bit. It runs slower than the normal code path but\n")
	TEXT("still faster as it avoids the many render target switches. (Default: 0)\n")
	TEXT("We want this enabled (1) on all 32 bit iOS devices (implemented through DeviceProfiles)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarBloomCross(
	TEXT("r.Bloom.Cross"),
	0.0f,
	TEXT("Experimental feature to give bloom kernel a more bright center sample (values between 1 and 3 work without causing aliasing)\n")
	TEXT("Existing bloom get lowered to match the same brightness\n")
	TEXT("<0 for a anisomorphic lens flare look (X only)\n")
	TEXT(" 0 off (default)\n")
	TEXT(">0 for a cross look (X and Y)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarAlphaChannel(
	TEXT("r.PostProcessing.PropagateAlpha"),
	0,
	TEXT("0 to disable scene alpha channel support in the post processing.\n")
	TEXT(" 0: disabled (default);\n")
	TEXT(" 1: enabled in linear color space;\n")
	TEXT(" 2: same as 1, but also enable it through the tonemapper. Compositing after the tonemapper is incorrect, as their is no meaning to tonemap the alpha channel. This is only meant to be use exclusively for broadcasting hardware that does not support linear color space compositing and tonemapping."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarPostProcessingPreferCompute(
	TEXT("r.PostProcessing.PreferCompute"),
	0,
	TEXT("Will use compute shaders for post processing where implementations available."),
	ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING)
static TAutoConsoleVariable<int32> CVarPostProcessingForceAsyncDispatch(
	TEXT("r.PostProcessing.ForceAsyncDispatch"),
	0,
	TEXT("Will force asynchronous dispatch for post processing compute shaders where implementations available.\n")
	TEXT("Only available for testing in non-shipping builds."),
	ECVF_RenderThreadSafe);
#endif

static TAutoConsoleVariable<int32> CVarTemporalAAAllowDownsampling(
	TEXT("r.TemporalAA.AllowDownsampling"),
	1,
	TEXT("Allows half-resolution color buffer to be produced during TAA. Only possible when motion blur is off and when using compute shaders for post processing."),
	ECVF_RenderThreadSafe);

// -------------------------------------------------------

bool ShouldDoComputePostProcessing(const FViewInfo& View)
{
	return CVarPostProcessingPreferCompute.GetValueOnRenderThread() && View.FeatureLevel >= ERHIFeatureLevel::SM5;
}

bool IsTemporalAASceneDownsampleAllowed(const FViewInfo& View)
{
	return CVarTemporalAAAllowDownsampling.GetValueOnRenderThread() != 0;
}

bool IsPostProcessOutputInHDR()
{
	static const auto CVarDumpFramesAsHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BufferVisualizationDumpFramesAsHDR"));

	return CVarDumpFramesAsHDR->GetValueOnRenderThread() != 0 || GetHighResScreenshotConfig().bCaptureHDR;
}

int32 GetPostProcessAAQuality()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessAAQuality"));

	return FMath::Clamp(CVar->GetValueOnAnyThread(), 0, 6);
}

class FSceneDownsampleChain
{
public:
	// The number of total stages in the chain.
	static const uint32 StageCount = 6;

	FSceneDownsampleChain() = default;

	void Init(
		FRDGBuilder& GraphBuilder,
		const FScreenPassViewInfo& ScreenPassView,
		const FEyeAdaptationParameters& EyeAdaptationParameters,
		FScreenPassTexture HalfResolutionSceneColor,
		EDownsampleQuality DownsampleQuality,
		bool bLogLumaInAlpha)
	{
		check(HalfResolutionSceneColor.IsValid());

		RDG_EVENT_SCOPE(GraphBuilder, "SceneDownsample");

		static const TCHAR* PassNames[StageCount] =
		{
			nullptr,
			TEXT("Scene(1/4)"),
			TEXT("Scene(1/8)"),
			TEXT("Scene(1/16)"),
			TEXT("Scene(1/32)"),
			TEXT("Scene(1/64)")
		};
		static_assert(ARRAY_COUNT(PassNames) == StageCount, "PassNames size must equal StageCount");

		// The first stage is the input.
		Textures[0] = HalfResolutionSceneColor;

		for (uint32 StageIndex = 1; StageIndex < StageCount; StageIndex++)
		{
			const uint32 PreviousStageIndex = StageIndex - 1;

			FDownsamplePassInputs PassInputs;
			PassInputs.Name = PassNames[StageIndex];
			PassInputs.SceneColor = Textures[PreviousStageIndex];
			PassInputs.Quality = DownsampleQuality;

			Textures[StageIndex] = AddDownsamplePass(GraphBuilder, ScreenPassView, PassInputs);

			if (bLogLumaInAlpha)
			{
				bLogLumaInAlpha = false;

				Textures[StageIndex] = AddBasicEyeAdaptationSetupPass(GraphBuilder, ScreenPassView, EyeAdaptationParameters, Textures[StageIndex]);
			}
		}

		bInitialized = true;
	}

	bool IsInitialized() const
	{
		return bInitialized;
	}

	FScreenPassTexture GetTexture(uint32 StageIndex) const
	{
		return Textures[StageIndex];
	}

	FScreenPassTexture GetFirstTexture() const
	{
		return Textures[0];
	}

	FScreenPassTexture GetLastTexture() const
	{
		return Textures[StageCount - 1];
	}

private:
	TStaticArray<FScreenPassTexture, StageCount> Textures;
	bool bInitialized = false;
};

enum class EBloomQuality : uint32
{
	Disabled,
	Q1,
	Q2,
	Q3,
	Q4,
	Q5,
	MAX
};

static_assert(
	static_cast<uint32>(EBloomQuality::MAX) == FSceneDownsampleChain::StageCount,
	"The total number of stages in the scene downsample chain and the number of bloom quality levels must match.");

EBloomQuality GetBloomQuality()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BloomQuality"));

	return static_cast<EBloomQuality>(FMath::Clamp(
		CVar->GetValueOnRenderThread(),
		static_cast<int32>(EBloomQuality::Disabled),
		static_cast<int32>(EBloomQuality::MAX)));
}

struct FBloomInputs
{
	FScreenPassTexture SceneColor;

	const FSceneDownsampleChain* SceneDownsampleChain = nullptr;
};

struct FBloomOutputs
{
	FScreenPassTexture SceneColor;
	FScreenPassTexture Bloom;
};

FBloomOutputs AddBloomPass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	const FBloomInputs& Inputs)
{
	check(Inputs.SceneColor.IsValid());
	check(Inputs.SceneDownsampleChain);

	const FFinalPostProcessSettings& Settings = ScreenPassView.View.FinalPostProcessSettings;

	const EBloomQuality BloomQuality = GetBloomQuality();

	FScreenPassTexture SceneColor = Inputs.SceneColor;
	FScreenPassTexture Bloom(GSystemTextures.GetBlackDummy(GraphBuilder));

	if (BloomQuality != EBloomQuality::Disabled)
	{
		const bool bFFTBloomEnabled = IsFFTBloomEnabled(ScreenPassView.View);

		if (bFFTBloomEnabled)
		{
			FScreenPassTexture FullResolution = Inputs.SceneColor;
			FScreenPassTexture HalfResolution = Inputs.SceneDownsampleChain->GetFirstTexture();

			FFFTBloomInputs PassInputs;
			PassInputs.FullResolutionTexture = FullResolution.Texture;
			PassInputs.FullResolutionViewRect = FullResolution.ViewRect;
			PassInputs.HalfResolutionTexture = HalfResolution.Texture;
			PassInputs.HalfResolutionViewRect = HalfResolution.ViewRect;

			SceneColor.Texture = AddFFTBloomPass(GraphBuilder, ScreenPassView.View, PassInputs);
		}
		else
		{
			RDG_EVENT_SCOPE(GraphBuilder, "Bloom");

			const float CrossBloom = CVarBloomCross.GetValueOnRenderThread();

			const FVector2D CrossCenterWeight(FMath::Max(CrossBloom, 0.0f), FMath::Abs(CrossBloom));

			check(BloomQuality != EBloomQuality::Disabled);
			const uint32 BloomQualityIndex = static_cast<uint32>(BloomQuality);
			const uint32 BloomQualityCountMax = static_cast<uint32>(EBloomQuality::MAX);

			struct FBloomStage
			{
				const float Size;
				const FLinearColor& Tint;
			};

			FBloomStage BloomStages[] =
			{
				{ Settings.Bloom6Size, Settings.Bloom6Tint },
				{ Settings.Bloom5Size, Settings.Bloom5Tint },
				{ Settings.Bloom4Size, Settings.Bloom4Tint },
				{ Settings.Bloom3Size, Settings.Bloom3Tint },
				{ Settings.Bloom2Size, Settings.Bloom2Tint },
				{ Settings.Bloom1Size, Settings.Bloom1Tint }
			};

			const uint32 BloomQualityToSceneDownsampleStage[] =
			{
				static_cast<uint32>(-1), // Disabled (sentinel entry to preserve indices)
				3, // Q1
				3, // Q2
				4, // Q3
				5, // Q4
				6  // Q5
			};

			static_assert(ARRAY_COUNT(BloomStages) == BloomQualityCountMax, "Array must be one less than the number of bloom quality entries.");
			static_assert(ARRAY_COUNT(BloomQualityToSceneDownsampleStage) == BloomQualityCountMax, "Array must be one less than the number of bloom quality entries.");

			// Use bloom quality to select the number of downsample stages to use for bloom.
			const uint32 BloomStageCount = BloomQualityToSceneDownsampleStage[BloomQualityIndex];

			const float TintScale = 1.0f / BloomQualityCountMax;

			for (uint32 StageIndex = 0, SourceIndex = BloomQualityCountMax - 1; StageIndex < BloomStageCount; ++StageIndex, --SourceIndex)
			{
				const FBloomStage& BloomStage = BloomStages[StageIndex];

				if (BloomStage.Size > SMALL_NUMBER)
				{
					FGaussianBlurInputs PassInputs;
					PassInputs.NameX = TEXT("BloomX");
					PassInputs.NameY = TEXT("BloomY");
					PassInputs.Filter = Inputs.SceneDownsampleChain->GetTexture(SourceIndex);
					PassInputs.Additive = Bloom;
					PassInputs.CrossCenterWeight = CrossCenterWeight;
					PassInputs.KernelSizePercent = BloomStage.Size * Settings.BloomSizeScale;
					PassInputs.TintColor = BloomStage.Tint * TintScale;

					Bloom = AddGaussianBlurPass(GraphBuilder, ScreenPassView, PassInputs);
				}
			}
		}
	}

	const ELensFlareQuality LensFlareQuality = GetLensFlareQuality();

	if (LensFlareQuality != ELensFlareQuality::Disabled &&
		!Settings.LensFlareTint.IsAlmostBlack() &&
		Settings.LensFlareBokehSize > SMALL_NUMBER &&
		Settings.LensFlareIntensity > SMALL_NUMBER)
	{
		FRHITexture* BokehTextureRHI = GWhiteTexture->TextureRHI;

		if (GEngine->DefaultBokehTexture)
		{
			FTextureResource* BokehTextureResource = GEngine->DefaultBokehTexture->Resource;

			if (BokehTextureResource && BokehTextureResource->TextureRHI)
			{
				BokehTextureRHI = BokehTextureResource->TextureRHI;
			}
		}

		if (Settings.LensFlareBokehShape)
		{
			FTextureResource* BokehTextureResource = Settings.LensFlareBokehShape->Resource;

			if (BokehTextureResource && BokehTextureResource->TextureRHI)
			{
				BokehTextureRHI = BokehTextureResource->TextureRHI;
			}
		}

		// The quality level controls which downsample stage we use as the flare input texture.
		const uint32 LensFlareDownsampleStageIndex = static_cast<uint32>(ELensFlareQuality::MAX) - static_cast<uint32>(LensFlareQuality) - 1;

		FLensFlareInputs LensFlareInputs;
		LensFlareInputs.Bloom = Bloom;
		LensFlareInputs.Flare = Inputs.SceneDownsampleChain->GetTexture(LensFlareDownsampleStageIndex);
		LensFlareInputs.BokehShapeTexture = BokehTextureRHI;
		LensFlareInputs.TintColorsPerFlare = Settings.LensFlareTints;
		LensFlareInputs.TintColor = Settings.LensFlareTint;
		LensFlareInputs.BokehSizePercent = Settings.LensFlareBokehSize;
		LensFlareInputs.Intensity = Settings.LensFlareIntensity;
		LensFlareInputs.Threshold = Settings.LensFlareThreshold;

		// If a bloom output texture isn't available, substitute the half resolution scene color instead, but disable bloom
		// composition. The pass needs a primary input in order to access the image descriptor and viewport for output.
		if (!LensFlareInputs.Bloom.Texture)
		{
			LensFlareInputs.Bloom = Inputs.SceneDownsampleChain->GetFirstTexture();
			LensFlareInputs.bCompositeWithBloom = false;
		}

		FScreenPassTexture LensFlare = AddLensFlaresPass(GraphBuilder, ScreenPassView, LensFlareInputs);

		if (LensFlare.Texture)
		{
			Bloom = LensFlare;
		}
	}

	FBloomOutputs PassOutputs;
	PassOutputs.SceneColor = SceneColor;
	PassOutputs.Bloom = Bloom;
	return PassOutputs;
}

FPostprocessContext::FPostprocessContext(FRHICommandListImmediate& InRHICmdList, FRenderingCompositionGraph& InGraph, const FViewInfo& InView)
: RHICmdList(InRHICmdList)
	, Graph(InGraph)
	, View(InView)
	, SceneColor(0)
	, SceneDepth(0)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(InRHICmdList);
	if(SceneContext.IsSceneColorAllocated())
	{
		SceneColor = Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.GetSceneColor()));
	}

	SceneDepth = Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.SceneDepthZ));

	FinalOutput = FRenderingCompositeOutputRef(SceneColor);
}

static FRCPassPostProcessTonemap* AddTonemapper(
	FPostprocessContext& Context,
	const FRenderingCompositeOutputRef& BloomOutputCombined,
	const FRenderingCompositeOutputRef& EyeAdaptation,
	const EAutoExposureMethod& EyeAdapationMethodId,
	const bool bDoGammaOnly,
	const bool bHDRTonemapperOutput)
{
	const FViewInfo& View = Context.View;
	const EStereoscopicPass StereoPass = View.StereoPass;

	FRenderingCompositeOutputRef TonemapperCombinedLUTOutputRef;
	if (IStereoRendering::IsAPrimaryView(StereoPass, GEngine->StereoRenderingDevice))
	{
		TonemapperCombinedLUTOutputRef = AddCombineLUTPass(Context.Graph);
	}

	const bool bDoEyeAdaptation = IsAutoExposureMethodSupported(View.GetFeatureLevel(), EyeAdapationMethodId);
	FRCPassPostProcessTonemap* PostProcessTonemap = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessTonemap(bDoGammaOnly, bDoEyeAdaptation, bHDRTonemapperOutput));

	PostProcessTonemap->SetInput(ePId_Input0, Context.FinalOutput);
	PostProcessTonemap->SetInput(ePId_Input1, BloomOutputCombined);
	PostProcessTonemap->SetInput(ePId_Input2, EyeAdaptation);
	PostProcessTonemap->SetInput(ePId_Input3, TonemapperCombinedLUTOutputRef);

	Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessTonemap);

	return PostProcessTonemap;
}

void FPostProcessing::AddGammaOnlyTonemapper(FPostprocessContext& Context)
{
	FRenderingCompositePass* PostProcessTonemap = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessTonemap(true, false/*eye*/, false));

	PostProcessTonemap->SetInput(ePId_Input0, Context.FinalOutput);

	Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessTonemap);
}

static bool AddPostProcessDepthOfFieldGaussian(FPostprocessContext& Context, FRenderingCompositeOutputRef& VelocityInput, FRenderingCompositeOutputRef& SeparateTranslucencyRef)
{
	// GaussianDOFPass performs Gaussian setup, blur and recombine.
	auto GaussianDOFPass = [&Context, &VelocityInput](FRenderingCompositeOutputRef& SeparateTranslucency, float FarSize, float NearSize)
	{
		// GenerateGaussianDOFBlur produces a blurred image from setup or potentially from taa result.
		auto GenerateGaussianDOFBlur = [&Context, &VelocityInput](FRenderingCompositeOutputRef& DOFSetup, bool bFarPass, float BlurSize)
		{
			FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

			const TCHAR* BlurDebugX = bFarPass ? TEXT("FarDOFBlurX") : TEXT("NearDOFBlurX");
			const TCHAR* BlurDebugY = bFarPass ? TEXT("FarDOFBlurY") : TEXT("NearDOFBlurY");

			return AddGaussianBlurPass(Context.Graph, BlurDebugX, BlurDebugY, DOFSetup, BlurSize);
		};

		const bool bFar = FarSize > 0.0f;
		const bool bNear = NearSize > 0.0f;
		const bool bCombinedNearFarPass = bFar && bNear;
		const bool bMobileQuality = Context.View.FeatureLevel < ERHIFeatureLevel::SM5;

		FRenderingCompositeOutputRef SetupInput(Context.FinalOutput);
		if (bMobileQuality)
		{
			const uint32 SetupInputDownsampleFactor = 1;

			SetupInput = AddDownsamplePass(Context.Graph, TEXT("GaussianSetupHalfRes"), SetupInput, SetupInputDownsampleFactor, EDownsampleQuality::High, EDownsampleFlags::ForceRaster, PF_FloatRGBA);
		}

		FRenderingCompositePass* DOFSetupPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDOFSetup(bFar, bNear));
		DOFSetupPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(SetupInput));
		DOFSetupPass->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.SceneDepth));
		FRenderingCompositeOutputRef DOFSetupFar(DOFSetupPass);
		FRenderingCompositeOutputRef DOFSetupNear(DOFSetupPass, bCombinedNearFarPass ? ePId_Output1 : ePId_Output0);

		FRenderingCompositeOutputRef DOFFarBlur, DOFNearBlur;
		if (bFar)
		{
			DOFFarBlur = GenerateGaussianDOFBlur(DOFSetupFar, true, FarSize);
		}

		if (bNear)
		{
			DOFNearBlur = GenerateGaussianDOFBlur(DOFSetupNear, false, NearSize);
		}

		FRenderingCompositePass* GaussianDOFRecombined = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDOFRecombine());
		GaussianDOFRecombined->SetInput(ePId_Input0, Context.FinalOutput);
		GaussianDOFRecombined->SetInput(ePId_Input1, DOFFarBlur);
		GaussianDOFRecombined->SetInput(ePId_Input2, DOFNearBlur);
		GaussianDOFRecombined->SetInput(ePId_Input3, SeparateTranslucency);

		Context.FinalOutput = FRenderingCompositeOutputRef(GaussianDOFRecombined);
	};

	float FarSize = Context.View.FinalPostProcessSettings.DepthOfFieldFarBlurSize;
	float NearSize = Context.View.FinalPostProcessSettings.DepthOfFieldNearBlurSize;
	const float MaxSize = CVarDepthOfFieldMaxSize.GetValueOnRenderThread();
	FarSize = FMath::Min(FarSize, MaxSize);
	NearSize = FMath::Min(NearSize, MaxSize);
	bool bFar = FarSize >= 0.01f;
	bool bNear = false;

	{
		const float CVarThreshold = CVarDepthOfFieldNearBlurSizeThreshold.GetValueOnRenderThread();
		bNear = (NearSize >= CVarThreshold);
	}

	if (Context.View.Family->EngineShowFlags.VisualizeDOF)
	{
		// no need for this pass
		bFar = false;
		bNear = false;
	}

	if (bFar || bNear)
	{
		GaussianDOFPass(SeparateTranslucencyRef, bFar ? FarSize : 0, bNear ? NearSize : 0);

		const bool bMobileQuality = Context.View.FeatureLevel < ERHIFeatureLevel::SM5;
		return SeparateTranslucencyRef.IsValid() && !bMobileQuality;
	}
	else
	{
		return false;
	}
}

// could be moved into the graph
// allows for Framebuffer blending optimization with the composition graph
void FPostProcessing::OverrideRenderTarget(FRenderingCompositeOutputRef It, TRefCountPtr<IPooledRenderTarget>& RT, FPooledRenderTargetDesc& Desc)
{
	for(;;)
	{
		It.GetOutput()->PooledRenderTarget = RT;
		It.GetOutput()->RenderTargetDesc = Desc;

		if(!It.GetPass()->FrameBufferBlendingWithInput0())
		{
			break;
		}

		It = *It.GetPass()->GetInput(ePId_Input0);
	}
}

bool FPostProcessing::AllowFullPostProcessing(const FViewInfo& View, ERHIFeatureLevel::Type FeatureLevel)
{
	if(FeatureLevel >= ERHIFeatureLevel::SM5)
	{
		return View.Family->EngineShowFlags.PostProcessing
			&& !View.Family->EngineShowFlags.VisualizeDistanceFieldAO
			&& !View.Family->EngineShowFlags.VisualizeDistanceFieldGI
			&& !View.Family->EngineShowFlags.VisualizeShadingModels
			&& !View.Family->EngineShowFlags.VisualizeMeshDistanceFields
			&& !View.Family->EngineShowFlags.VisualizeGlobalDistanceField
			&& !View.Family->EngineShowFlags.ShaderComplexity;
	}
	else
	{
		// Mobile post processing
		return View.Family->EngineShowFlags.PostProcessing
			&& !View.Family->EngineShowFlags.ShaderComplexity;
	}
}

namespace
{

class FComposeSeparateTranslucencyPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FComposeSeparateTranslucencyPS);
	SHADER_USE_PARAMETER_STRUCT(FComposeSeparateTranslucencyPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColor)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SeparateTranslucency)
		SHADER_PARAMETER_SAMPLER(SamplerState, SeparateTranslucencySampler)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FComposeSeparateTranslucencyPS, "/Engine/Private/ComposeSeparateTranslucency.usf", "MainPS", SF_Pixel);

FRDGTextureRef AddSeparateTranslucencyCompositionPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef SceneColor, FRDGTextureRef SeparateTranslucency)
{
	FRDGTextureDesc SceneColorDesc = SceneColor->Desc;
	SceneColorDesc.TargetableFlags &= ~TexCreate_UAV;
	SceneColorDesc.TargetableFlags |= TexCreate_RenderTargetable;

	FRDGTextureRef NewSceneColor = GraphBuilder.CreateTexture(SceneColorDesc, TEXT("SceneColor"));

	FComposeSeparateTranslucencyPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComposeSeparateTranslucencyPS::FParameters>();
	PassParameters->SceneColor = SceneColor;
	PassParameters->SceneColorSampler = TStaticSamplerState<SF_Point>::GetRHI();
	PassParameters->SeparateTranslucency = SeparateTranslucency;
	PassParameters->SeparateTranslucencySampler = TStaticSamplerState<SF_Point>::GetRHI();
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(NewSceneColor, ERenderTargetLoadAction::ENoAction);

	TShaderMapRef<FComposeSeparateTranslucencyPS> PixelShader(View.ShaderMap);
	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		View.ShaderMap,
		RDG_EVENT_NAME("ComposeSeparateTranslucency %dx%d", View.ViewRect.Width(), View.ViewRect.Height()),
		*PixelShader,
		PassParameters,
		View.ViewRect);

	return NewSceneColor;
}

} // namespace

void FPostProcessing::Process(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderPostProcessing);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PostProcessing_Process);

	check(IsInRenderingThread());
	check(View.VerifyMembersChecks());

	const ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();

	check(FeatureLevel >= ERHIFeatureLevel::SM5);

	GRenderTargetPool.AddPhaseEvent(TEXT("PostProcessing"));

	// All post processing is happening on the render thread side. All passes can access FinalPostProcessSettings and all
	// view settings. Those are copies for the RT then never get access by the main thread again.
	// Pointers to other structures might be unsafe to touch.

	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

		FRenderingCompositeOutputRef PreFlattenVelocity;
		FRenderingCompositeOutputRef SeparateTranslucency;

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// Register textures as inputs into the composition graph.
		if (SceneContext.SeparateTranslucencyRT)
		{
			SeparateTranslucency = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.SeparateTranslucencyRT));

			// make sure we only release if this is the last view we're rendering
			if (View.IsLastInFamily())
			{
				// the node keeps another reference so the RT will not be release too early
				SceneContext.FreeSeparateTranslucency();
			}
		}
		else
		{
			FRenderingCompositePass* NodeSeparateTranslucency = Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessInput(FSceneRenderTargets::Get(RHICmdList).GetSeparateTranslucencyDummy()));
			SeparateTranslucency = FRenderingCompositeOutputRef(NodeSeparateTranslucency);
		}

		if (VelocityRT)
		{
			PreFlattenVelocity = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(VelocityRT));
		}

		const FEngineShowFlags& EngineShowFlags = View.Family->EngineShowFlags;

		const bool bVisualizeHDR = EngineShowFlags.VisualizeHDR;

		const bool bViewFamilyOutputInHDR = GRHISupportsHDROutput && IsHDREnabled();

		const bool bVisualizeGBufferOverview = IsVisualizeGBufferOverviewEnabled(View);

		const bool bVisualizeGBufferDumpToFile = IsVisualizeGBufferDumpToFileEnabled(View);

		const bool bOutputInHDR = IsPostProcessOutputInHDR();

		const FPaniniProjectionConfig PaniniConfig(View);

		// GBuffers are released prior to executing the composition graph. We take a reference here
		// and then release the reference inside of RDGPass. This allows RDGPass to control lifetime
		// of the GBuffers internally.
		SceneContext.AdjustGBufferRefCount(RHICmdList, 1);

		FRenderingCompositePass* RDGPass = Context.Graph.RegisterPass(
			new(FMemStack::Get()) TRCPassForRDG<3, 4>(
				[this, &View, &EngineShowFlags, &SceneContext, bVisualizeHDR, bViewFamilyOutputInHDR, bOutputInHDR, bVisualizeGBufferOverview, bVisualizeGBufferDumpToFile, PaniniConfig]
		(FRenderingCompositePass* Pass, FRenderingCompositePassContext& InContext)
		{
			FRDGBuilder GraphBuilder(InContext.RHICmdList);

			const FScreenPassViewInfo& ScreenPassView(View);

			const FIntRect PrimaryViewRect = View.ViewRect;

			FRDGTextureRef ViewFamilyTexture = Pass->FindRDGTextureForOutput(GraphBuilder, ePId_Output0, TEXT("ViewFamilyTexture"));

			// Scene color is updated incrementally through the post process pipeline.
			FScreenPassTexture SceneColor(Pass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input0, TEXT("SceneColor")), PrimaryViewRect);

			// Unprocessed scene color stores the original input.
			const FScreenPassTexture OriginalSceneColor = SceneColor;

			const FScreenPassTexture SceneDepth(Pass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input1, TEXT("SceneDepth")), PrimaryViewRect);

			const FScreenPassTexture SeparateTranslucency(Pass->CreateRDGTextureForOptionalInput(GraphBuilder, ePId_Input2, TEXT("SeparateTranslucency")), PrimaryViewRect);

			const FScreenPassTexture CustomDepth(GraphBuilder.TryRegisterExternalTexture(SceneContext.CustomDepth, TEXT("CustomDepth")), PrimaryViewRect);

			const FScreenPassTexture BlackDummy(GSystemTextures.GetBlackDummy(GraphBuilder), FIntRect(0, 0, 1, 1));

			FScreenPassTexture SceneColorBeforeTonemap;
			FScreenPassTexture SceneColorAfterTonemap;

			FSceneTextureParameters SceneTextures;
			SetupSceneTextureParameters(GraphBuilder, &SceneTextures);

			// Fallback to a black texture if no velocity.
			if (!SceneTextures.SceneVelocityBuffer)
			{
				SceneTextures.SceneVelocityBuffer = BlackDummy.Texture;
			}

			const FScreenPassTexture Velocity(SceneTextures.SceneVelocityBuffer, PrimaryViewRect);

			// Histogram defaults to black because the histogram eye adaptation pass is used for the manual metering mode.
			FRDGTextureRef HistogramTexture = BlackDummy.Texture;

			FEyeAdaptationParameters EyeAdaptationParameters = GetEyeAdaptationParameters(View, ERHIFeatureLevel::SM5);

			// Default the new eye adaptation to the last one in case it's not generated this frame.
			FRDGTextureRef LastEyeAdaptationTexture = GetEyeAdaptationTexture(GraphBuilder, View);
			FRDGTextureRef EyeAdaptationTexture = LastEyeAdaptationTexture;

			enum class EPass : uint32
			{
				Tonemap,
				FXAA,
				PostProcessMaterialAfterTonemapping,
				VisualizeDepthOfField,
				VisualizeStationaryLightOverlap,
				VisualizeLightCulling,
				SelectionOutline,
				EditorPrimitive,
				VisualizeShadingModels,
				VisualizeGBufferHints,
				VisualizeSubsurface,
				VisualizeGBufferOverview,
				VisualizeHDR,
				PixelInspector,
				HMDDistortion,
				HighResolutionScreenshotMask,
				PrimaryUpscale,
				SecondaryUpscale,
				MAX
			};

			const TCHAR* PassNames[] =
			{
				TEXT("Tonemap"),
				TEXT("FXAA"),
				TEXT("PostProcessMaterial (AfterTonemapping)"),
				TEXT("VisualizeDepthOfField"),
				TEXT("VisualizeStationaryLightOverlap"),
				TEXT("VisualizeLightCulling"),
				TEXT("SelectionOutline"),
				TEXT("EditorPrimitive"),
				TEXT("VisualizeShadingModels"),
				TEXT("VisualizeGBufferHints"),
				TEXT("VisualizeSubsurface"),
				TEXT("VisualizeGBufferOverview"),
				TEXT("VisualizeHDR"),
				TEXT("PixelInspector"),
				TEXT("HMDDistortion"),
				TEXT("HighResolutionScreenshotMask"),
				TEXT("PrimaryUpscale"),
				TEXT("SecondaryUpscale")
			};

			static_assert(static_cast<uint32>(EPass::MAX) == ARRAY_COUNT(PassNames), "EPass does not match PassNames.");

			const FScreenPassRenderTarget ViewFamilyOutput(
				ViewFamilyTexture,
				// Raw output mode uses the original view rect. Otherwise the final unscaled rect is used.
				View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::RawOutput ? View.ViewRect : View.UnscaledViewRect,
				// First view clears the view family texture; all remaining views load.
				View.IsFirstInFamily() ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad);

			/**
			 * The override pass sequence represents the final set of passes which can receive a view family render target override.
			 * This is necessary to avoid a copy at the end of the chain. The pass sequence helper will compute the final pass and
			 * seamlessly handle the override. To do so, the order of passes and whether they are enabled needs to be calculated up
			 * front. When adding a new pass, make sure to preserve ordering and respect the enable flag during execution. The pass
			 * sequence helper will validate correctness at runtime.
			 */
			TOverridePassSequence<EPass> PassSequence(ViewFamilyOutput);
			PassSequence.SetNames(PassNames, ARRAY_COUNT(PassNames));
			PassSequence.SetEnabled(EPass::VisualizeStationaryLightOverlap, EngineShowFlags.StationaryLightOverlap);
			PassSequence.SetEnabled(EPass::VisualizeLightCulling, EngineShowFlags.VisualizeLightCulling);
#if WITH_EDITOR
			PassSequence.SetEnabled(EPass::SelectionOutline, GIsEditor && EngineShowFlags.SelectionOutline && !EngineShowFlags.Wireframe && !bVisualizeHDR);
			PassSequence.SetEnabled(EPass::EditorPrimitive, FSceneRenderer::ShouldCompositeEditorPrimitives(View));
#else
			PassSequence.SetEnabled(EPass::SelectionOutline, false);
			PassSequence.SetEnabled(EPass::EditorPrimitive, false);
#endif
			PassSequence.SetEnabled(EPass::VisualizeShadingModels, EngineShowFlags.VisualizeShadingModels);
			PassSequence.SetEnabled(EPass::VisualizeGBufferHints, EngineShowFlags.GBufferHints);
			PassSequence.SetEnabled(EPass::VisualizeSubsurface, EngineShowFlags.VisualizeSSS);
			PassSequence.SetEnabled(EPass::VisualizeGBufferOverview, bVisualizeGBufferOverview || bVisualizeGBufferDumpToFile);
			PassSequence.SetEnabled(EPass::VisualizeHDR, EngineShowFlags.VisualizeHDR);
#if WITH_EDITOR
			PassSequence.SetEnabled(EPass::PixelInspector, View.bUsePixelInspector);
#else
			PassSequence.SetEnabled(EPass::PixelInspector, false);
#endif
			PassSequence.SetEnabled(EPass::HMDDistortion, EngineShowFlags.StereoRendering && EngineShowFlags.HMDDistortion);
			PassSequence.SetEnabled(EPass::HighResolutionScreenshotMask, IsHighResolutionScreenshotMaskEnabled(View));
			PassSequence.SetEnabled(EPass::PrimaryUpscale, PaniniConfig.IsEnabled() || (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::SpatialUpscale && PrimaryViewRect.Size() != View.GetSecondaryViewRectSize()));
			PassSequence.SetEnabled(EPass::SecondaryUpscale, View.RequiresSecondaryUpscale());

			if (AllowFullPostProcessing(View, View.GetFeatureLevel()))
			{
				const auto GetPostProcessMaterialInputs = [CustomDepth, SeparateTranslucency, Velocity] (FScreenPassTexture SceneColor)
				{
					FPostProcessMaterialInputs Inputs;
					Inputs.SetInput(EPostProcessMaterialInput::SceneColor, SceneColor);
					Inputs.SetInput(EPostProcessMaterialInput::SeparateTranslucency, SeparateTranslucency);
					Inputs.SetInput(EPostProcessMaterialInput::Velocity, Velocity);
					Inputs.CustomDepthTexture = CustomDepth.Texture;
					return Inputs;
				};

				const EAutoExposureMethod AutoExposureMethod = GetAutoExposureMethod(View);

				const EDownsampleQuality DownsampleQuality = GetDownsampleQuality();

				const EPixelFormat DownsampleOverrideFormat = PF_FloatRGB;

				const bool bPreferCompute = ShouldDoComputePostProcessing(View);

				const bool bPrimaryView = IStereoRendering::IsAPrimaryView(View.StereoPass, GEngine->StereoRenderingDevice);

				const bool bHasViewState = View.ViewState != nullptr;

				const bool bDepthOfFieldEnabled = DiaphragmDOF::IsEnabled(View);

				const bool bVisualizeDepthOfField = bDepthOfFieldEnabled && EngineShowFlags.VisualizeDOF;

				const bool bVisualizeMotionBlur = IsVisualizeMotionBlurEnabled(View);

				const EAntiAliasingMethod AntiAliasingMethod = !bVisualizeDepthOfField ? View.AntiAliasingMethod : AAM_None;

				// Motion blur gets replaced by the visualization pass.
				const bool bMotionBlurEnabled = !bVisualizeMotionBlur && IsMotionBlurEnabled(View);

				// Skip tonemapping for visualizers which overwrite the HDR scene color.
				const bool bTonemapEnabled = !bVisualizeMotionBlur;

				const bool bTonemapOutputInHDR = View.Family->SceneCaptureSource == SCS_FinalColorHDR || bOutputInHDR || bViewFamilyOutputInHDR;

				// We don't test for the EyeAdaptation engine show flag here. If disabled, the auto exposure pass is still executes but performs a clamp.
				const bool bEyeAdaptationEnabled =
					// Skip for transient views.
					bHasViewState &&
					// Skip for secondary views in a stereo setup.
					bPrimaryView;

				const bool bHistogramEnabled =
					// Force the histogram on when we are visualizing HDR.
					bVisualizeHDR ||
					// Skip if not using histogram eye adaptation.
					(bEyeAdaptationEnabled && AutoExposureMethod == EAutoExposureMethod::AEM_Histogram &&
					// Skip if we don't have any exposure range to generate (eye adaptation will clamp).
					View.FinalPostProcessSettings.AutoExposureMinBrightness < View.FinalPostProcessSettings.AutoExposureMaxBrightness);

				const bool bBloomEnabled = View.FinalPostProcessSettings.BloomIntensity > 0.0f;

				const FPostProcessMaterialChain PostProcessMaterialAfterTonemappingChain = GetPostProcessMaterialChain(View, BL_AfterTonemapping);

				PassSequence.SetEnabled(EPass::Tonemap, bTonemapEnabled);
				PassSequence.SetEnabled(EPass::FXAA, AntiAliasingMethod == AAM_FXAA);
				PassSequence.SetEnabled(EPass::PostProcessMaterialAfterTonemapping, PostProcessMaterialAfterTonemappingChain.Num() != 0);
				PassSequence.SetEnabled(EPass::VisualizeDepthOfField, bVisualizeDepthOfField);
				PassSequence.Finalize();

				// Post Process Material Chain - Before Translucency
				{
					const FPostProcessMaterialChain MaterialChain = GetPostProcessMaterialChain(View, BL_BeforeTranslucency);

					if (MaterialChain.Num())
					{
						SceneColor = AddPostProcessMaterialChain(GraphBuilder, ScreenPassView, GetPostProcessMaterialInputs(SceneColor), MaterialChain);
					}
				}

				// Diaphragm Depth of Field
				{
					FRDGTextureRef LocalSceneColorTexture = SceneColor.Texture;

					if (bDepthOfFieldEnabled)
					{
						LocalSceneColorTexture = DiaphragmDOF::AddPasses(GraphBuilder, SceneTextures, View, SceneColor.Texture, SeparateTranslucency.Texture);
					}

					// DOF passes were not added, therefore need to compose Separate translucency manually.
					if (LocalSceneColorTexture == SceneColor.Texture && SeparateTranslucency.Texture)
					{
						LocalSceneColorTexture = AddSeparateTranslucencyCompositionPass(GraphBuilder, View, SceneColor.Texture, SeparateTranslucency.Texture);
					}

					SceneColor.Texture = LocalSceneColorTexture;
				}

				// Post Process Material Chain - Before Tonemapping
				{
					const FPostProcessMaterialChain MaterialChain = GetPostProcessMaterialChain(View, BL_BeforeTonemapping);

					if (MaterialChain.Num())
					{
						SceneColor = AddPostProcessMaterialChain(GraphBuilder, ScreenPassView, GetPostProcessMaterialInputs(SceneColor), MaterialChain);
					}
				}

				FScreenPassTexture HalfResolutionSceneColor;

				// Scene color view rectangle after temporal AA upscale to secondary screen percentage.
				FIntRect SecondaryViewRect = PrimaryViewRect;

				// Temporal Anti-aliasing. Also may perform a temporal upsample from primary to secondary view rect.
				if (AntiAliasingMethod == AAM_TemporalAA)
				{
					// Whether we allow the temporal AA pass to downsample scene color. It may choose not to based on internal context,
					// in which case the output half resolution texture will remain null.
					const bool bAllowSceneDownsample =
						IsTemporalAASceneDownsampleAllowed(View) &&
						// We can only merge if the normal downsample pass would happen immediately after.
						!bMotionBlurEnabled && !bVisualizeMotionBlur &&
						// TemporalAA is only able to match the low quality mode (box filter).
						GetDownsampleQuality() == EDownsampleQuality::Low;

					AddTemporalAAPass(
						GraphBuilder,
						SceneTextures,
						ScreenPassView,
						bAllowSceneDownsample,
						DownsampleOverrideFormat,
						SceneColor.Texture,
						&SceneColor.Texture,
						&SecondaryViewRect,
						&HalfResolutionSceneColor.Texture,
						&HalfResolutionSceneColor.ViewRect);
				}

				//! SceneColorTexture is now upsampled to the SecondaryViewRect. Use SecondaryViewRect for input / output.
				SceneColor.ViewRect = SecondaryViewRect;

				// Post Process Material Chain - SSR Input
				if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
				{
					const FPostProcessMaterialChain MaterialChain = GetPostProcessMaterialChain(View, BL_SSRInput);

					if (MaterialChain.Num())
					{
						// Save off SSR post process output for the next frame.
						FScreenPassTexture PassOutput = AddPostProcessMaterialChain(GraphBuilder, ScreenPassView, GetPostProcessMaterialInputs(SceneColor), MaterialChain);
						GraphBuilder.QueueTextureExtraction(PassOutput.Texture, &View.ViewState->PrevFrameViewInfo.CustomSSRInput);
					}
				}

				// Motion blur visualization replaces motion blur when enabled.
				if (bVisualizeMotionBlur)
				{
					check(Velocity.ViewRect == SceneDepth.ViewRect);

					SceneColor.Texture = AddVisualizeMotionBlurPass(
						GraphBuilder,
						ScreenPassView,
						SceneColor.ViewRect,
						SceneDepth.ViewRect,
						SceneColor.Texture,
						SceneDepth.Texture,
						Velocity.Texture);
				}
				else if (bMotionBlurEnabled)
				{
					check(Velocity.ViewRect == SceneDepth.ViewRect);

					SceneColor.Texture = AddMotionBlurPass(
						GraphBuilder,
						ScreenPassView,
						SceneColor.ViewRect,
						SceneDepth.ViewRect,
						SceneColor.Texture,
						SceneDepth.Texture,
						Velocity.Texture);
				}

				// If TAA didn't do it, downsample the scene color texture by half.
				if (!HalfResolutionSceneColor.Texture)
				{
					FDownsamplePassInputs PassInputs;
					PassInputs.Name = TEXT("HalfResolutionSceneColor");
					PassInputs.SceneColor = SceneColor;
					PassInputs.Quality = DownsampleQuality;
					PassInputs.FormatOverride = DownsampleOverrideFormat;

					HalfResolutionSceneColor = AddDownsamplePass(GraphBuilder, ScreenPassView, PassInputs);
				}

				FSceneDownsampleChain SceneDownsampleChain;

				if (bHistogramEnabled)
				{
					HistogramTexture = AddHistogramPass(GraphBuilder, ScreenPassView, EyeAdaptationParameters, HalfResolutionSceneColor, LastEyeAdaptationTexture);
				}

				const bool bBasicEyeAdaptationEnabled = bEyeAdaptationEnabled && (AutoExposureMethod == EAutoExposureMethod::AEM_Basic);

				const bool bBloomThresholdEnabled = View.FinalPostProcessSettings.BloomThreshold > 0.0f;

				if (bEyeAdaptationEnabled)
				{
					if (bBasicEyeAdaptationEnabled)
					{
						const bool bLogLumaInAlpha = true;

						SceneDownsampleChain.Init(
							GraphBuilder,
							ScreenPassView,
							EyeAdaptationParameters,
							HalfResolutionSceneColor,
							DownsampleQuality,
							bLogLumaInAlpha);

						// Use the alpha channel in the last downsample (smallest) to compute eye adaptations values.
						EyeAdaptationTexture = AddBasicEyeAdaptationPass(GraphBuilder, ScreenPassView, EyeAdaptationParameters, SceneDownsampleChain.GetLastTexture(), LastEyeAdaptationTexture);
					}
					// Add histogram eye adaptation pass even if no histogram exists to support the manual clamping mode.
					else
					{
						EyeAdaptationTexture = AddHistogramEyeAdaptationPass(GraphBuilder, ScreenPassView, EyeAdaptationParameters, HistogramTexture);
					}
				}

				FScreenPassTexture Bloom = BlackDummy;

				if (bBloomEnabled)
				{
					FSceneDownsampleChain BloomDownsampleChain;

					FBloomInputs PassInputs;
					PassInputs.SceneColor = SceneColor;

					// Reuse the main scene downsample chain if a threshold isn't required for bloom. 
					if (SceneDownsampleChain.IsInitialized() && !bBloomThresholdEnabled)
					{
						PassInputs.SceneDownsampleChain = &SceneDownsampleChain;
					}
					else
					{
						FScreenPassTexture DownsampleInput = HalfResolutionSceneColor;

						if (bBloomThresholdEnabled)
						{
							const float BloomThreshold = View.FinalPostProcessSettings.BloomThreshold;

							DownsampleInput = AddBloomSetupPass(GraphBuilder, ScreenPassView, DownsampleInput, EyeAdaptationTexture, BloomThreshold);
						}

						const bool bLogLumaInAlpha = false;

						BloomDownsampleChain.Init(
							GraphBuilder,
							ScreenPassView,
							EyeAdaptationParameters,
							DownsampleInput,
							DownsampleQuality,
							bLogLumaInAlpha);

						PassInputs.SceneDownsampleChain = &BloomDownsampleChain;
					}

					FBloomOutputs PassOutputs = AddBloomPass(GraphBuilder, ScreenPassView, PassInputs);
					SceneColor = PassOutputs.SceneColor;
					Bloom = PassOutputs.Bloom;
				}

				SceneColorBeforeTonemap = SceneColor;

				if (PassSequence.IsEnabled(EPass::Tonemap))
				{
					const FPostProcessMaterialChain MaterialChain = GetPostProcessMaterialChain(View, BL_ReplacingTonemapper);

					if (MaterialChain.Num())
					{
						const UMaterialInterface* HighestPriorityMaterial = MaterialChain[0];

						FPostProcessMaterialInputs PassInputs;
						PassSequence.AcceptOverrideIfLastPass(EPass::Tonemap, PassInputs.OverrideOutput);
						PassInputs.SetInput(EPostProcessMaterialInput::SceneColor, SceneColor);
						PassInputs.SetInput(EPostProcessMaterialInput::SeparateTranslucency, SeparateTranslucency);
						PassInputs.SetInput(EPostProcessMaterialInput::CombinedBloom, Bloom);
						PassInputs.CustomDepthTexture = CustomDepth.Texture;

						SceneColor = AddPostProcessMaterialPass(GraphBuilder, ScreenPassView, PassInputs, HighestPriorityMaterial);
					}
					else
					{
						FRDGTextureRef ColorGradingTexture = nullptr;

						if (bPrimaryView)
						{
							ColorGradingTexture = AddCombineLUTPass(GraphBuilder, ScreenPassView);
						}
						else
						{
							ColorGradingTexture = GraphBuilder.TryRegisterExternalTexture(View.GetTonemappingLUT());
						}

						FTonemapInputs PassInputs;
						PassSequence.AcceptOverrideIfLastPass(EPass::Tonemap, PassInputs.OverrideOutput);
						PassInputs.SceneColor = SceneColor;
						PassInputs.Bloom = Bloom;
						PassInputs.EyeAdaptationTexture = EyeAdaptationTexture;
						PassInputs.ColorGradingTexture = ColorGradingTexture;
						PassInputs.bWriteAlphaChannel = AntiAliasingMethod == AAM_FXAA || FPostProcessing::HasAlphaChannelSupport();
						PassInputs.bOutputInHDR = bTonemapOutputInHDR;

						SceneColor = AddTonemapPass(GraphBuilder, ScreenPassView, PassInputs);
					}
				}

				SceneColorAfterTonemap = SceneColor;

				if (PassSequence.IsEnabled(EPass::FXAA))
				{
					FFXAAInputs PassInputs;
					PassSequence.AcceptOverrideIfLastPass(EPass::FXAA, PassInputs.OverrideOutput);
					PassInputs.SceneColor = SceneColor;
					PassInputs.Quality = GetFXAAQuality();

					SceneColor = AddFXAAPass(GraphBuilder, ScreenPassView, PassInputs);
				}

				// Post Process Material Chain - After Tonemapping
				if (PassSequence.IsEnabled(EPass::PostProcessMaterialAfterTonemapping))
				{
					FPostProcessMaterialInputs PassInputs = GetPostProcessMaterialInputs(SceneColor);
					PassSequence.AcceptOverrideIfLastPass(EPass::PostProcessMaterialAfterTonemapping, PassInputs.OverrideOutput);

					SceneColor = AddPostProcessMaterialChain(GraphBuilder, ScreenPassView, PassInputs, PostProcessMaterialAfterTonemappingChain);
				}

				if (PassSequence.IsEnabled(EPass::VisualizeDepthOfField))
				{
					FVisualizeDOFInputs PassInputs;
					PassSequence.AcceptOverrideIfLastPass(EPass::VisualizeDepthOfField, PassInputs.OverrideOutput);
					PassInputs.SceneColor = SceneColor;
					PassInputs.SceneDepth = SceneDepth;

					SceneColor = AddVisualizeDOFPass(GraphBuilder, ScreenPassView, PassInputs);
				}
			}
			// Minimal PostProcessing - Separate translucency composition and gamma-correction only.
			else
			{
				PassSequence.SetEnabled(EPass::Tonemap, true);
				PassSequence.SetEnabled(EPass::FXAA, false);
				PassSequence.SetEnabled(EPass::PostProcessMaterialAfterTonemapping, false);
				PassSequence.SetEnabled(EPass::VisualizeDepthOfField, false);
				PassSequence.Finalize();

				if (SeparateTranslucency.Texture)
				{
					SceneColor.Texture = AddSeparateTranslucencyCompositionPass(GraphBuilder, View, SceneColor.Texture, SeparateTranslucency.Texture);
				}

				SceneColorBeforeTonemap = SceneColor;

				if (PassSequence.IsEnabled(EPass::Tonemap))
				{
					FTonemapInputs PassInputs;
					PassSequence.AcceptOverrideIfLastPass(EPass::Tonemap, PassInputs.OverrideOutput);
					PassInputs.SceneColor = SceneColor;
					PassInputs.bOutputInHDR = bViewFamilyOutputInHDR;
					PassInputs.bGammaOnly = true;
				
					SceneColor = AddTonemapPass(GraphBuilder, ScreenPassView, PassInputs);
				}

				SceneColorAfterTonemap = SceneColor;
			}

			if (PassSequence.IsEnabled(EPass::VisualizeStationaryLightOverlap))
			{
				ensureMsgf(View.PrimaryScreenPercentageMethod != EPrimaryScreenPercentageMethod::TemporalUpscale, TEXT("TAAU should be disabled when visualizing stationary light overlap."));

				FVisualizeComplexityInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::VisualizeStationaryLightOverlap, PassInputs.OverrideOutput);
				PassInputs.SceneColor = OriginalSceneColor;
				PassInputs.Colors = GEngine->StationaryLightOverlapColors;
				PassInputs.ColorSamplingMethod = FVisualizeComplexityInputs::EColorSamplingMethod::Ramp;

				SceneColor = AddVisualizeComplexityPass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (PassSequence.IsEnabled(EPass::VisualizeLightCulling))
			{
				ensureMsgf(View.PrimaryScreenPercentageMethod != EPrimaryScreenPercentageMethod::TemporalUpscale, TEXT("TAAU should be disabled when visualizing light culling."));

				// 0.1f comes from the values used in LightAccumulator_GetResult
				const float ComplexityScale = 1.0f / (float)(GEngine->LightComplexityColors.Num() - 1) / 0.1f;

				FVisualizeComplexityInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::VisualizeLightCulling, PassInputs.OverrideOutput);
				PassInputs.SceneColor = OriginalSceneColor;
				PassInputs.Colors = GEngine->LightComplexityColors;
				PassInputs.ColorSamplingMethod = FVisualizeComplexityInputs::EColorSamplingMethod::Linear;
				PassInputs.ComplexityScale = ComplexityScale;

				SceneColor = AddVisualizeComplexityPass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (EngineShowFlags.VisualizeLPV)
			{
				AddVisualizeLPVPass(GraphBuilder, ScreenPassView.View, SceneColor);
			}

#if WITH_EDITOR
			if (PassSequence.IsEnabled(EPass::SelectionOutline))
			{
				FSelectionOutlineInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::SelectionOutline, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.SceneDepth = SceneDepth;

				SceneColor = AddSelectionOutlinePass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (PassSequence.IsEnabled(EPass::EditorPrimitive))
			{
				FEditorPrimitiveInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::EditorPrimitive, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.SceneDepth = SceneDepth;
				PassInputs.BasePassType = FEditorPrimitiveInputs::EBasePassType::Deferred;

				SceneColor = AddEditorPrimitivePass(GraphBuilder, ScreenPassView, PassInputs);
			}
#endif

			if (PassSequence.IsEnabled(EPass::VisualizeShadingModels))
			{
				ensureMsgf(View.PrimaryScreenPercentageMethod != EPrimaryScreenPercentageMethod::TemporalUpscale, TEXT("TAAU should be disabled when visualizing shading models."));

				FVisualizeShadingModelInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::VisualizeShadingModels, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.SceneTextures = &SceneTextures;

				SceneColor = AddVisualizeShadingModelPass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (PassSequence.IsEnabled(EPass::VisualizeGBufferHints))
			{
				ensureMsgf(View.PrimaryScreenPercentageMethod != EPrimaryScreenPercentageMethod::TemporalUpscale, TEXT("TAAU should be disabled when visualizing gbuffer hints."));

				FVisualizeGBufferHintsInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::VisualizeGBufferHints, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.OriginalSceneColor = OriginalSceneColor;
				PassInputs.SceneTextures = &SceneTextures;

				SceneColor = AddVisualizeGBufferHintsPass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (PassSequence.IsEnabled(EPass::VisualizeSubsurface))
			{
				ensureMsgf(View.PrimaryScreenPercentageMethod != EPrimaryScreenPercentageMethod::TemporalUpscale, TEXT("TAAU should be disabled when visualizing subsurface."));

				FVisualizeSubsurfaceInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::VisualizeSubsurface, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.SceneTextures = &SceneTextures;

				SceneColor = AddVisualizeSubsurfacePass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (PassSequence.IsEnabled(EPass::VisualizeGBufferOverview))
			{
				FVisualizeGBufferOverviewInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::VisualizeGBufferOverview, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.SceneColorBeforeTonemap = SceneColorBeforeTonemap;
				PassInputs.SceneColorAfterTonemap = SceneColorAfterTonemap;
				PassInputs.SeparateTranslucency = SeparateTranslucency;
				PassInputs.Velocity = Velocity;
				PassInputs.bOverview = bVisualizeGBufferOverview;
				PassInputs.bDumpToFile = bVisualizeGBufferDumpToFile;
				PassInputs.bOutputInHDR = bOutputInHDR;

				SceneColor = AddVisualizeGBufferOverviewPass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (PassSequence.IsEnabled(EPass::VisualizeHDR))
			{
				FVisualizeHDRInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::VisualizeHDR, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.SceneColorBeforeTonemap = SceneColorBeforeTonemap;
				PassInputs.HistogramTexture = HistogramTexture;
				PassInputs.EyeAdaptationTexture = EyeAdaptationTexture;
				PassInputs.EyeAdaptationParameters = &EyeAdaptationParameters;

				SceneColor = AddVisualizeHDRPass(GraphBuilder, ScreenPassView, PassInputs);
			}

#if WITH_EDITOR
			if (PassSequence.IsEnabled(EPass::PixelInspector))
			{
				FPixelInspectorInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::PixelInspector, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.SceneColorBeforeTonemap = SceneColorBeforeTonemap;
				PassInputs.OriginalSceneColor = OriginalSceneColor;
				PassInputs.SceneTextures = &SceneTextures;

				SceneColor = AddPixelInspectorPass(GraphBuilder, View, PassInputs);
			}
#endif

			if (PassSequence.IsEnabled(EPass::HMDDistortion))
			{
				FHMDDistortionInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::HMDDistortion, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;

				SceneColor = AddHMDDistortionPass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (EngineShowFlags.TestImage)
			{
				AddTestImagePass(GraphBuilder, ScreenPassView, SceneColor);
			}

			if (ShaderPrint::IsEnabled() && ShaderPrint::IsSupported(View))
			{
				ShaderPrint::DrawView(GraphBuilder, View, SceneColor.Texture);
			}

			if (PassSequence.IsEnabled(EPass::HighResolutionScreenshotMask))
			{
				FHighResolutionScreenshotMaskInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::HighResolutionScreenshotMask, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.Material = View.FinalPostProcessSettings.HighResScreenshotMaterial;
				PassInputs.MaskMaterial = View.FinalPostProcessSettings.HighResScreenshotMaskMaterial;
				PassInputs.CaptureRegionMaterial = View.FinalPostProcessSettings.HighResScreenshotCaptureRegionMaterial;

				SceneColor = AddHighResolutionScreenshotMaskPass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (PassSequence.IsEnabled(EPass::PrimaryUpscale))
			{
				FUpscaleInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::PrimaryUpscale, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.Method = GetUpscaleMethod();
				PassInputs.Stage = PassSequence.IsEnabled(EPass::SecondaryUpscale) ? EUpscaleStage::PrimaryToSecondary : EUpscaleStage::PrimaryToOutput;

				// Panini projection is handled by the primary upscale pass.
				PassInputs.PaniniConfig = PaniniConfig;

				SceneColor = AddUpscalePass(GraphBuilder, ScreenPassView, PassInputs);
			}

			if (PassSequence.IsEnabled(EPass::SecondaryUpscale))
			{
				FUpscaleInputs PassInputs;
				PassSequence.AcceptOverrideIfLastPass(EPass::SecondaryUpscale, PassInputs.OverrideOutput);
				PassInputs.SceneColor = SceneColor;
				PassInputs.Method = View.Family->SecondaryScreenPercentageMethod == ESecondaryScreenPercentageMethod::LowerPixelDensitySimulation ? EUpscaleMethod::SmoothStep : EUpscaleMethod::Nearest;
				PassInputs.Stage = EUpscaleStage::SecondaryToOutput;

				SceneColor = AddUpscalePass(GraphBuilder, ScreenPassView, PassInputs);
			}

			// Release held GBuffer reference taken during composition graph setup. Passes will take their own references during RDG setup.
			SceneContext.AdjustGBufferRefCount(InContext.RHICmdList, -1);

			// We only release the SceneColor after the last view was processed (SplitScreen)
			if (View.IsLastInFamily() && !GIsEditor)
			{
				// The RT should be released as early as possible to allow sharing of that memory for other purposes.
				// This becomes even more important with some limited VRam (XBoxOne).
				FSceneRenderTargets::Get(InContext.RHICmdList).SetSceneColor(nullptr);
			}

			Pass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, SceneColor.Texture);

			GraphBuilder.Execute();

			InContext.SceneColorViewRect = SceneColor.ViewRect;
			InContext.ReferenceBufferSize = SceneColor.Texture->Desc.Extent;
		}));

		RDGPass->SetInput(ePId_Input0, Context.FinalOutput);
		RDGPass->SetInput(ePId_Input1, Context.SceneDepth);
		RDGPass->SetInput(ePId_Input2, SeparateTranslucency);

		Context.FinalOutput = FRenderingCompositeOutputRef(RDGPass, ePId_Output0);

		// Add a pass-through for the final step if a backbuffer UAV is required but unsupported by this RHI
		if (Context.FinalOutput.IsComputePass() && !View.Family->RenderTarget->GetRenderTargetUAV().IsValid())
		{
			FRenderingCompositePass* PassthroughNode = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessPassThrough(nullptr));
			PassthroughNode->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(PassthroughNode);
		}

		// After the graph is built but before the graph is processed.
		// If a postprocess material is using a GBuffer it adds the refcount int FRCPassPostProcessMaterial::Process()
		// and when it gets processed it removes the refcount
		// We only release the GBuffers after the last view was processed (SplitScreen)
		if(View.IsLastInFamily())
		{
			// Generally we no longer need the GBuffers, anyone that wants to keep the GBuffers for longer should have called AdjustGBufferRefCount(1) to keep it for longer
			// and call AdjustGBufferRefCount(-1) once it's consumed. This needs to happen each frame. PostProcessMaterial do that automatically
			FSceneRenderTargets::Get(RHICmdList).AdjustGBufferRefCount(RHICmdList, -1);
		}

		// The graph setup should be finished before this line ----------------------------------------
		{
			// currently created on the heap each frame but View.Family->RenderTarget could keep this object and all would be cleaner
			TRefCountPtr<IPooledRenderTarget> Temp;
			FSceneRenderTargetItem Item;
			Item.TargetableTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();
			Item.ShaderResourceTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();
			Item.UAV = View.Family->RenderTarget->GetRenderTargetUAV();

			FPooledRenderTargetDesc Desc;

			// Texture could be bigger than viewport
			if (View.Family->RenderTarget->GetRenderTargetTexture())
			{
				Desc.Extent.X = View.Family->RenderTarget->GetRenderTargetTexture()->GetSizeX();
				Desc.Extent.Y = View.Family->RenderTarget->GetRenderTargetTexture()->GetSizeY();
			}
			else
			{
				Desc.Extent = View.Family->RenderTarget->GetSizeXY();
			}

			const bool bIsFinalOutputComputePass = Context.FinalOutput.IsComputePass();
			Desc.TargetableFlags |= bIsFinalOutputComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
			Desc.Format = bIsFinalOutputComputePass ? PF_R8G8B8A8 : PF_B8G8R8A8;

			// todo: this should come from View.Family->RenderTarget
			Desc.Format = bViewFamilyOutputInHDR ? GRHIHDRDisplayOutputFormat : Desc.Format;
			if (View.Family->SceneCaptureSource == SCS_FinalColorHDR) Desc.Format = PF_FloatRGBA;
			Desc.NumMips = 1;
			Desc.DebugName = TEXT("FinalPostProcessColor");

			GRenderTargetPool.CreateUntrackedElement(Desc, Temp, Item);

			OverrideRenderTarget(Context.FinalOutput, Temp, Desc);

			TArray<FRenderingCompositePass*> TargetedRoots;
			TargetedRoots.Add(Context.FinalOutput.GetPass());

			// execute the graph/DAG
			CompositeContext.Process(TargetedRoots, TEXT("PostProcessing"));

			// May need to wait on the final pass to complete
			if (Context.FinalOutput.IsAsyncComputePass())
			{
				FRHIComputeFence* ComputeFinalizeFence = Context.FinalOutput.GetComputePassEndFence();
				if (ComputeFinalizeFence)
				{
					Context.RHICmdList.WaitComputeFence(ComputeFinalizeFence);
				}
			}
		}
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("AfterPostprocessing"));
}

static bool IsGaussianActive(FPostprocessContext& Context)
{

	float FarSize = Context.View.FinalPostProcessSettings.DepthOfFieldFarBlurSize;
	float NearSize = Context.View.FinalPostProcessSettings.DepthOfFieldNearBlurSize;

	float MaxSize = CVarDepthOfFieldMaxSize.GetValueOnRenderThread();

	FarSize = FMath::Min(FarSize, MaxSize);
	NearSize = FMath::Min(NearSize, MaxSize);
	const float CVarThreshold = CVarDepthOfFieldNearBlurSizeThreshold.GetValueOnRenderThread();

	if ((FarSize < 0.01f) && (NearSize < CVarThreshold))
	{
		return false;
	}
	return true;
}

void FPostProcessing::ProcessES2(FRHICommandListImmediate& RHICmdList, FScene* Scene, const FViewInfo& View)
{
	check(IsInRenderingThread());

	// This page: https://udn.epicgames.com/Three/RenderingOverview#Rendering%20state%20defaults 
	// describes what state a pass can expect and to what state it need to be set back.

	// All post processing is happening on the render thread side. All passes can access FinalPostProcessSettings and all
	// view settings. Those are copies for the RT then never get access by the main thread again.
	// Pointers to other structures might be unsafe to touch.

	const EDebugViewShaderMode DebugViewShaderMode = View.Family->GetDebugViewShaderMode();
	bool bAllowFullPostProcess =
		!(
			DebugViewShaderMode == DVSM_ShaderComplexity ||
			DebugViewShaderMode == DVSM_ShaderComplexityContainedQuadOverhead ||
			DebugViewShaderMode == DVSM_ShaderComplexityBleedingQuadOverhead
		);

	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);
		FRenderingCompositeOutputRef BloomOutput;
		FRenderingCompositeOutputRef DofOutput;

		bool bUseAa = View.AntiAliasingMethod == AAM_TemporalAA;

		// AA with Mobile32bpp mode requires this outside of bUsePost.
		if(bUseAa)
		{
			// Handle pointer swap for double buffering.
			FSceneViewState* ViewState = (FSceneViewState*)View.State;
			if(ViewState)
			{
				// Note that this drops references to the render targets from two frames ago. This
				// causes them to be added back to the pool where we can grab them again.
				ViewState->MobileAaBloomSunVignette1 = ViewState->MobileAaBloomSunVignette0;
				ViewState->MobileAaColor1 = ViewState->MobileAaColor0;
			}
		}

		const FIntPoint FinalTargetSize = View.Family->RenderTarget->GetSizeXY();
		FIntRect FinalOutputViewRect = View.ViewRect;
		FIntPoint PrePostSourceViewportSize = View.ViewRect.Size();
		// ES2 preview uses a subsection of the scene RT
		FIntPoint SceneColorSize = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
		bool bViewRectSource = SceneColorSize != PrePostSourceViewportSize;
		bool bMobileHDR32bpp = IsMobileHDR32bpp();

		// temporary solution for SP_METAL using HW sRGB flag during read vs all other mob platforms using
		// incorrect UTexture::SRGB state. (UTexture::SRGB != HW texture state)
		bool bSRGBAwareTarget = View.Family->RenderTarget->GetDisplayGamma() == 1.0f
			&& View.bIsSceneCapture
			&& IsMetalMobilePlatform(View.GetShaderPlatform());

		// add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ---------
		if( View.Family->EngineShowFlags.PostProcessing && bAllowFullPostProcess)
		{
			const EMobileHDRMode HDRMode = GetMobileHDRMode();
			bool bUseEncodedHDR = HDRMode == EMobileHDRMode::EnabledRGBE;
			bool bHDRModeAllowsPost = bUseEncodedHDR || HDRMode == EMobileHDRMode::EnabledFloat16;

			bool bUseSun = !bUseEncodedHDR && View.bLightShaftUse;
			bool bUseDof = !bUseEncodedHDR && GetMobileDepthOfFieldScale(View) > 0.0f && !Context.View.Family->EngineShowFlags.VisualizeDOF;
			bool bUseBloom = View.FinalPostProcessSettings.BloomIntensity > 0.0f;
			bool bUseVignette = View.FinalPostProcessSettings.VignetteIntensity > 0.0f;

			bool bWorkaround = CVarRenderTargetSwitchWorkaround.GetValueOnRenderThread() != 0;

			// Use original mobile Dof on ES2 devices regardless of bMobileHQGaussian.
			// HQ gaussian 
#if PLATFORM_HTML5 // EMSCRITPEN_TOOLCHAIN_UPGRADE_CHECK -- i.e. remove this when LLVM no longer errors -- appologies for the mess
			// UE-61742 : the following will coerce i160 bit (bMobileHQGaussian) to an i8 LLVM variable
			bool bUseMobileDof = bUseDof && ((1 - View.FinalPostProcessSettings.bMobileHQGaussian) + (Context.View.GetFeatureLevel() < ERHIFeatureLevel::ES3_1));
#else
			bool bUseMobileDof = bUseDof && (!View.FinalPostProcessSettings.bMobileHQGaussian || (Context.View.GetFeatureLevel() < ERHIFeatureLevel::ES3_1));
#endif

			// This is a workaround to avoid a performance cliff when using many render targets. 
			bool bUseBloomSmall = bUseBloom && !bUseSun && !bUseDof && bWorkaround;

			// Post is not supported on ES2 devices using mosaic.
			bool bUsePost = bHDRModeAllowsPost && IsMobileHDR();
			
			if (bUsePost && IsMobileDistortionActive(View))
			{
				FRenderingCompositePass* AccumulatedDistortion = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCDistortionAccumulatePassES2(SceneColorSize, Scene));
				AccumulatedDistortion->SetInput(ePId_Input0, Context.FinalOutput); // unused atm
				FRenderingCompositeOutputRef AccumulatedDistortionRef(AccumulatedDistortion);
				
				FRenderingCompositePass* PostProcessDistorsion = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCDistortionMergePassES2(SceneColorSize));
				PostProcessDistorsion->SetInput(ePId_Input0, Context.FinalOutput);
				PostProcessDistorsion->SetInput(ePId_Input1, AccumulatedDistortionRef);
				Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessDistorsion);
			}

			// Always evaluate custom post processes
			if (bUsePost)
			{
				Context.FinalOutput = AddPostProcessMaterialChain(Context, BL_BeforeTranslucency, nullptr);
				Context.FinalOutput = AddPostProcessMaterialChain(Context, BL_BeforeTonemapping, nullptr);
			}

			// Optional fixed pass processes
			if (bUsePost && (bUseSun | bUseDof | bUseBloom | bUseVignette))
			{
				if (bUseSun || bUseDof)
				{
					// Convert depth to {circle of confusion, sun shaft intensity}
				//	FRenderingCompositePass* PostProcessSunMask = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMaskES2(PrePostSourceViewportSize, false));
					FRenderingCompositePass* PostProcessSunMask = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMaskES2(SceneColorSize));
					PostProcessSunMask->SetInput(ePId_Input0, Context.FinalOutput);
					Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessSunMask);
					//@todo Ronin sunmask pass isnt clipping to image only.
				}

				FRenderingCompositeOutputRef PostProcessBloomSetup;
				if (bUseSun || bUseMobileDof || bUseBloom)
				{
					if(bUseBloomSmall)
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomSetupSmallES2(PrePostSourceViewportSize, bViewRectSource));
						Pass->SetInput(ePId_Input0, Context.FinalOutput);
						PostProcessBloomSetup = FRenderingCompositeOutputRef(Pass);
					}
					else
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomSetupES2(FinalOutputViewRect, bViewRectSource));
						Pass->SetInput(ePId_Input0, Context.FinalOutput);
						PostProcessBloomSetup = FRenderingCompositeOutputRef(Pass);
					}
				}

				if (bUseDof)
				{
					if (bUseMobileDof)
					{
						// Near dilation circle of confusion size.
						// Samples at 1/16 area, writes to 1/16 area.
						FRenderingCompositeOutputRef PostProcessNear;
						{
							FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDofNearES2(FinalOutputViewRect.Size()));
							Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
							PostProcessNear = FRenderingCompositeOutputRef(Pass);
						}

						// DOF downsample pass.
						// Samples at full resolution, writes to 1/4 area.
						FRenderingCompositeOutputRef PostProcessDofDown;
						{
							FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDofDownES2(FinalOutputViewRect, bViewRectSource));
							Pass->SetInput(ePId_Input0, Context.FinalOutput);
							Pass->SetInput(ePId_Input1, PostProcessNear);
							PostProcessDofDown = FRenderingCompositeOutputRef(Pass);
						}

						// DOF blur pass.
						// Samples at 1/4 area, writes to 1/4 area.
						FRenderingCompositeOutputRef PostProcessDofBlur;
						{
							FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDofBlurES2(FinalOutputViewRect.Size()));
							Pass->SetInput(ePId_Input0, PostProcessDofDown);
							Pass->SetInput(ePId_Input1, PostProcessNear);
							PostProcessDofBlur = FRenderingCompositeOutputRef(Pass);
							DofOutput = PostProcessDofBlur;
						}
					}
					else
					{
						// black is how we clear the velocity buffer so this means no velocity
						FRenderingCompositePass* NoVelocity = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSystemTextures.BlackDummy));
						FRenderingCompositeOutputRef NoVelocityRef(NoVelocity);
						
						bool bDepthOfField = 
							View.Family->EngineShowFlags.DepthOfField &&
							IsGaussianActive(Context);

						if(bDepthOfField)
						{
							FRenderingCompositeOutputRef DummySeparateTranslucency;
							AddPostProcessDepthOfFieldGaussian(Context, NoVelocityRef, DummySeparateTranslucency);
						}
					}
				}

				// Bloom.
				FRenderingCompositeOutputRef PostProcessDownsample2;
				FRenderingCompositeOutputRef PostProcessDownsample3;
				FRenderingCompositeOutputRef PostProcessDownsample4;
				FRenderingCompositeOutputRef PostProcessDownsample5;
				FRenderingCompositeOutputRef PostProcessUpsample4;
				FRenderingCompositeOutputRef PostProcessUpsample3;
				FRenderingCompositeOutputRef PostProcessUpsample2;

				if(bUseBloomSmall)
				{
					float DownScale = 0.66f * 4.0f;
					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/4, DownScale * 2.0f));
						Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
						PostProcessDownsample2 = FRenderingCompositeOutputRef(Pass);
					}
				}

				if(bUseBloom && (!bUseBloomSmall))
				{
					float DownScale = 0.66f * 4.0f;
					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/4, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
						PostProcessDownsample2 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/8, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample2);
						PostProcessDownsample3 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/16, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample3);
						PostProcessDownsample4 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/32, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample4);
						PostProcessDownsample5 = FRenderingCompositeOutputRef(Pass);
					}

					const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

					float UpScale = 0.66f * 2.0f;
					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom4Tint.R, Settings.Bloom4Tint.G, Settings.Bloom4Tint.B, 0.0f);
						FVector4 TintB = FVector4(Settings.Bloom5Tint.R, Settings.Bloom5Tint.G, Settings.Bloom5Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						TintB *= View.FinalPostProcessSettings.BloomIntensity;
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/32, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample4);
						Pass->SetInput(ePId_Input1, PostProcessDownsample5);
						PostProcessUpsample4 = FRenderingCompositeOutputRef(Pass);
					}

					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom3Tint.R, Settings.Bloom3Tint.G, Settings.Bloom3Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						FVector4 TintB = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/16, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample3);
						Pass->SetInput(ePId_Input1, PostProcessUpsample4);
						PostProcessUpsample3 = FRenderingCompositeOutputRef(Pass);
					}

					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom2Tint.R, Settings.Bloom2Tint.G, Settings.Bloom2Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						// Scaling Bloom2 by extra factor to match filter area difference between PC default and mobile.
						TintA *= 0.5;
						FVector4 TintB = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/8, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample2);
						Pass->SetInput(ePId_Input1, PostProcessUpsample3);
						PostProcessUpsample2 = FRenderingCompositeOutputRef(Pass);
					}
				}

				FRenderingCompositeOutputRef PostProcessSunBlur;
				if(bUseSun)
				{
					// Sunshaft depth blur using downsampled alpha.
					FRenderingCompositeOutputRef PostProcessSunAlpha;
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunAlphaES2(PrePostSourceViewportSize));
						Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
						PostProcessSunAlpha = FRenderingCompositeOutputRef(Pass);
					}

					// Sunshaft blur number two.
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunBlurES2(PrePostSourceViewportSize));
						Pass->SetInput(ePId_Input0, PostProcessSunAlpha);
						PostProcessSunBlur = FRenderingCompositeOutputRef(Pass);
					}
				}

				if(bUseSun | bUseVignette | bUseBloom)
				{
					FRenderingCompositeOutputRef PostProcessSunMerge;
					if(bUseBloomSmall) 
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMergeSmallES2(PrePostSourceViewportSize));
						Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
						Pass->SetInput(ePId_Input1, PostProcessDownsample2);
						PostProcessSunMerge = FRenderingCompositeOutputRef(Pass);
						BloomOutput = PostProcessSunMerge;
					}
					else
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMergeES2(PrePostSourceViewportSize));
						if(bUseSun)
						{
							Pass->SetInput(ePId_Input0, PostProcessSunBlur);
						}
						if(bUseBloom)
						{
							Pass->SetInput(ePId_Input1, PostProcessBloomSetup);
							Pass->SetInput(ePId_Input2, PostProcessUpsample2);
						}
						PostProcessSunMerge = FRenderingCompositeOutputRef(Pass);
						BloomOutput = PostProcessSunMerge;
					}

					// Mobile temporal AA requires a composite of two of these frames.
					if(bUseAa && (bUseBloom || bUseSun))
					{
						FSceneViewState* ViewState = (FSceneViewState*)View.State;
						FRenderingCompositeOutputRef PostProcessSunMerge2;
						if(ViewState && ViewState->MobileAaBloomSunVignette1)
						{
							FRenderingCompositePass* History;
							History = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(ViewState->MobileAaBloomSunVignette1));
							PostProcessSunMerge2 = FRenderingCompositeOutputRef(History);
						}
						else
						{
							PostProcessSunMerge2 = PostProcessSunMerge;
						}

						FRenderingCompositeOutputRef PostProcessSunAvg;
						{
							FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunAvgES2(PrePostSourceViewportSize));
							Pass->SetInput(ePId_Input0, PostProcessSunMerge);
							Pass->SetInput(ePId_Input1, PostProcessSunMerge2);
							PostProcessSunAvg = FRenderingCompositeOutputRef(Pass);
						}
						BloomOutput = PostProcessSunAvg;
					}
				}
			} // bUsePost

			// mobile separate translucency 
			if (IsMobileSeparateTranslucencyActive(Context.View))
			{
				FRCSeparateTranslucensyPassES2* Pass = (FRCSeparateTranslucensyPassES2*)Context.Graph.RegisterPass(new(FMemStack::Get()) FRCSeparateTranslucensyPassES2());
				Pass->SetInput(ePId_Input0, Context.FinalOutput);
				Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
			}
		}
		
		static const auto VarTonemapperFilm = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.TonemapperFilm"));
		const bool bUseTonemapperFilm = Context.View.GetFeatureLevel() == ERHIFeatureLevel::ES3_1 && IsMobileHDR() && !bMobileHDR32bpp && GSupportsRenderTargetFormat_PF_FloatRGBA && (VarTonemapperFilm && VarTonemapperFilm->GetValueOnRenderThread());


		static const auto VarTonemapperUpscale = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileTonemapperUpscale"));
		bool bDisableUpscaleInTonemapper = IsMobileHDRMosaic() || !VarTonemapperUpscale || VarTonemapperUpscale->GetValueOnRenderThread() == 0;

		bool* DoScreenPercentageInTonemapperPtr = nullptr;
		FRenderingCompositePass* TonemapperPass = nullptr;
		if (bAllowFullPostProcess)
		{
			if (bUseTonemapperFilm)
			{
				//@todo Ronin Set to EAutoExposureMethod::AEM_Basic for PC vk crash.
				FRCPassPostProcessTonemap* PostProcessTonemap = AddTonemapper(Context, BloomOutput, nullptr, EAutoExposureMethod::AEM_Histogram, false, false);
				// remember the tonemapper pass so we can check if it's last
				TonemapperPass = PostProcessTonemap;

				PostProcessTonemap->bDoScreenPercentageInTonemapper = false;
				DoScreenPercentageInTonemapperPtr = &PostProcessTonemap->bDoScreenPercentageInTonemapper;			
			}
			else
			{
				// Must run to blit to back buffer even if post processing is off.
				FRCPassPostProcessTonemapES2* PostProcessTonemap = (FRCPassPostProcessTonemapES2*)Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessTonemapES2(Context.View, bViewRectSource, bSRGBAwareTarget));
				// remember the tonemapper pass so we can check if it's last
				TonemapperPass = PostProcessTonemap;

				PostProcessTonemap->SetInput(ePId_Input0, Context.FinalOutput);
				if (!BloomOutput.IsValid())
				{
					FRenderingCompositePass* NoBloom = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSystemTextures.BlackAlphaOneDummy));
					FRenderingCompositeOutputRef NoBloomRef(NoBloom);
					PostProcessTonemap->SetInput(ePId_Input1, NoBloomRef);
				}
				else
				{
					PostProcessTonemap->SetInput(ePId_Input1, BloomOutput);
				}
				PostProcessTonemap->SetInput(ePId_Input2, DofOutput);

				Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessTonemap);

				PostProcessTonemap->bDoScreenPercentageInTonemapper = false;
				DoScreenPercentageInTonemapperPtr = &PostProcessTonemap->bDoScreenPercentageInTonemapper;
			}
			SetMobilePassFlipVerticalAxis(TonemapperPass);
		}

		// if Context.FinalOutput was the clipped result of sunmask stage then this stage also restores Context.FinalOutput back original target size.
		FinalOutputViewRect = View.UnscaledViewRect;

		if (View.Family->EngineShowFlags.PostProcessing && bAllowFullPostProcess)
		{
			if (IsMobileHDR() && !IsMobileHDRMosaic())
			{
				Context.FinalOutput = AddPostProcessMaterialChain(Context, BL_AfterTonemapping, nullptr);
			}
			SetMobilePassFlipVerticalAxis(Context.FinalOutput.GetPass());

			if (bUseAa)
			{
				// Double buffer post output.
				FSceneViewState* ViewState = (FSceneViewState*)View.State;

				FRenderingCompositeOutputRef PostProcessPrior = Context.FinalOutput;
				if(ViewState && ViewState->MobileAaColor1)
				{
					FRenderingCompositePass* History;
					History = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(ViewState->MobileAaColor1));
					PostProcessPrior = FRenderingCompositeOutputRef(History);
				}

				// Mobile temporal AA is done after tonemapping.
				FRenderingCompositePass* PostProcessAa = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAaES2());
				PostProcessAa->SetInput(ePId_Input0, Context.FinalOutput);
				PostProcessAa->SetInput(ePId_Input1, PostProcessPrior);
				Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessAa);
			}
		}

		if (IsHighResolutionScreenshotMaskEnabled(View))
		{
			AddHighResScreenshotMask(Context);
		}

#if WITH_EDITOR
		// Show the selection outline if it is in the editor and we aren't in wireframe 
		// If the engine is in demo mode and game view is on we also do not show the selection outline
		if ( GIsEditor
			&& View.Family->EngineShowFlags.SelectionOutline
			&& !(View.Family->EngineShowFlags.Wireframe)
			)
		{
			Context.FinalOutput = AddSelectionOutlinePass(Context.Graph, Context.FinalOutput);
		}

		if (FSceneRenderer::ShouldCompositeEditorPrimitives(View))
		{
			Context.FinalOutput = AddEditorPrimitivePass(Context.Graph, Context.FinalOutput, FEditorPrimitiveInputs::EBasePassType::Mobile);
		}
#endif

		// Apply ScreenPercentage
		if (View.UnscaledViewRect != View.ViewRect)
		{
			if (bDisableUpscaleInTonemapper || Context.FinalOutput.GetPass() != TonemapperPass)
			{
				Context.FinalOutput = AddUpscalePass(Context.Graph, Context.FinalOutput, EUpscaleMethod::Bilinear, EUpscaleStage::PrimaryToOutput);
			}
			else if (DoScreenPercentageInTonemapperPtr)
			{
				*DoScreenPercentageInTonemapperPtr = true;
			}
		}

#ifdef WITH_EDITOR
		bool bES2Legend = true;
#else
		// Legend is costly so we don't do it for ES2, ideally we make a shader permutation
		bool bES2Legend = false;
#endif

		if(DebugViewShaderMode == DVSM_QuadComplexity)
		{
			Context.FinalOutput = AddVisualizeComplexityPass(
				Context.Graph,
				Context.FinalOutput,
				GEngine->QuadComplexityColors,
				FVisualizeComplexityInputs::EColorSamplingMethod::Stair,
				1.0f, bES2Legend);
		}

		if(DebugViewShaderMode == DVSM_ShaderComplexity || DebugViewShaderMode == DVSM_ShaderComplexityContainedQuadOverhead || DebugViewShaderMode == DVSM_ShaderComplexityBleedingQuadOverhead)
		{
			Context.FinalOutput = AddVisualizeComplexityPass(
				Context.Graph,
				Context.FinalOutput,
				GEngine->ShaderComplexityColors,
				FVisualizeComplexityInputs::EColorSamplingMethod::Ramp,
				1.0f, bES2Legend);
		}

		if (View.Family->EngineShowFlags.StereoRendering && View.Family->EngineShowFlags.HMDDistortion)
		{
			Context.FinalOutput = AddHMDDistortionPass(Context.Graph, Context.FinalOutput);
		}

		// The graph setup should be finished before this line ----------------------------------------

		{
			// currently created on the heap each frame but View.Family->RenderTarget could keep this object and all would be cleaner
			TRefCountPtr<IPooledRenderTarget> Temp;
			FSceneRenderTargetItem Item;
			Item.TargetableTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();
			Item.ShaderResourceTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();

			FPooledRenderTargetDesc Desc;

			if (View.Family->RenderTarget->GetRenderTargetTexture())
			{
				Desc.Extent.X = View.Family->RenderTarget->GetRenderTargetTexture()->GetSizeX();
				Desc.Extent.Y = View.Family->RenderTarget->GetRenderTargetTexture()->GetSizeY();
			}
			else
			{
				Desc.Extent = View.Family->RenderTarget->GetSizeXY();
			}

			// todo: this should come from View.Family->RenderTarget
			Desc.Format = PF_B8G8R8A8;
			Desc.NumMips = 1;
			Desc.DebugName = TEXT("OverriddenRenderTarget");
			Desc.TargetableFlags |= TexCreate_RenderTargetable;

			GRenderTargetPool.CreateUntrackedElement(Desc, Temp, Item);

			OverrideRenderTarget(Context.FinalOutput, Temp, Desc);

			CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("PostProcessingES2"));
		}
	}
	SetMobilePassFlipVerticalAxis(nullptr);
}

void FPostProcessing::ProcessPlanarReflection(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& OutFilteredSceneColor)
{
	FSceneViewState* ViewState = View.ViewState;
	const EAntiAliasingMethod AntiAliasingMethod = View.AntiAliasingMethod;

	const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	if (AntiAliasingMethod == AAM_TemporalAA)
	{
		check(ViewState);

		FRDGBuilder GraphBuilder(RHICmdList);

		FSceneTextureParameters SceneTextures;
		SetupSceneTextureParameters(GraphBuilder, &SceneTextures);

		// Planar reflections don't support velocity.
		SceneTextures.SceneVelocityBuffer = nullptr;

		const FTemporalAAHistory& InputHistory = View.PrevViewInfo.TemporalAAHistory;
		FTemporalAAHistory* OutputHistory = &ViewState->PrevFrameViewInfo.TemporalAAHistory;

		FTAAPassParameters Parameters(View);
		Parameters.SceneColorInput = GraphBuilder.RegisterExternalTexture(SceneContext.GetSceneColor(), TEXT("SceneColor"));

		FTAAOutputs PassOutputs = AddTemporalAAPass(
			GraphBuilder,
			SceneTextures,
			View,
			Parameters,
			InputHistory,
			OutputHistory);

		GraphBuilder.QueueTextureExtraction(PassOutputs.SceneColor, &OutFilteredSceneColor);

		GraphBuilder.Execute();
	}
	else
	{
		OutFilteredSceneColor = SceneContext.GetSceneColor();
	}
}

bool FPostProcessing::HasAlphaChannelSupport()
{
	return CVarAlphaChannel.GetValueOnAnyThread() != 0;
}
