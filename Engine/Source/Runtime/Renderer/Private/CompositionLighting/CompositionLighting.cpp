// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CompositionLighting.cpp: The center for all deferred lighting activities.
=============================================================================*/

#include "CompositionLighting/CompositionLighting.h"
#include "ScenePrivate.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessInput.h"
#include "PostProcess/PostProcessing.h"
#include "CompositionLighting/PostProcessLpvIndirect.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "LightPropagationVolumeSettings.h"
#include "DecalRenderingShared.h"
#include "VisualizeTexture.h"
#include "RayTracing/RaytracingOptions.h"
#include "SceneTextureParameters.h"

/** The global center for all deferred lighting activities. */
FCompositionLighting GCompositionLighting;

DECLARE_GPU_STAT_NAMED(CompositionBeforeBasePass, TEXT("Composition BeforeBasePass") );
DECLARE_GPU_STAT_NAMED(CompositionPreLighting, TEXT("Composition PreLighting") );
DECLARE_GPU_STAT_NAMED(CompositionLpvIndirect, TEXT("Composition LpvIndirect") );
DECLARE_GPU_STAT_NAMED(CompositionPostLighting, TEXT("Composition PostLighting") );

static TAutoConsoleVariable<int32> CVarSSAOSmoothPass(
	TEXT("r.AmbientOcclusion.Compute.Smooth"),
	1,
	TEXT("Whether to smooth SSAO output when TAA is disabled"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarGTAODownsample(
	TEXT("r.GTAO.Downsample"),
	0,
	TEXT("Perform GTAO at Halfres \n ")
	TEXT("0: Off \n ")
	TEXT("1: On (default)\n "),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarGTAOTemporalFilter(
	TEXT("r.GTAO.TemporalFilter"),
	1,
	TEXT("Enable Temporal Filter for GTAO \n ")
	TEXT("0: Off \n ")
	TEXT("1: On (default)\n "),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarGTAOSpatialFilter(
	TEXT("r.GTAO.SpatialFilter"),
	1,
	TEXT("Enable Spatial Filter for GTAO \n ")
	TEXT("0: Off \n ")
	TEXT("1: On (default)\n "),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarGTAOCombined(
	TEXT("r.GTAO.Combined"),
	1,
	TEXT("Enable Spatial Filter for GTAO \n ")
	TEXT("0: Off \n ")
	TEXT("1: On (default)\n "),
	ECVF_RenderThreadSafe | ECVF_Scalability);

bool IsAmbientCubemapPassRequired(const FSceneView& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;

	return View.FinalPostProcessSettings.ContributingCubemaps.Num() != 0 && IsUsingGBuffers(View.GetShaderPlatform());
}

bool IsLpvIndirectPassRequired(const FViewInfo& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;

	const FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if(ViewState)
	{
		// This check should be inclusive to stereo views
		const bool bIncludeStereoViews = true;

		FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel(), bIncludeStereoViews);

		if(LightPropagationVolume)
		{
			const FLightPropagationVolumeSettings& LPVSettings = View.FinalPostProcessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();

			if(LPVSettings.LPVIntensity > 0.0f)
			{
				return true;
			}
		}
	}

	return false;
}

static bool IsReflectionEnvironmentActive(const FSceneView& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;

	// LPV & Screenspace Reflections : Reflection Environment active if either LPV (assumed true if this was called), Reflection Captures or SSR active

	bool IsReflectingEnvironment = View.Family->EngineShowFlags.ReflectionEnvironment;
	bool HasReflectionCaptures = (Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() > 0);
	bool HasSSR = View.Family->EngineShowFlags.ScreenSpaceReflections;

	return (Scene->GetFeatureLevel() == ERHIFeatureLevel::SM5 && IsReflectingEnvironment && (HasReflectionCaptures || HasSSR) && !IsAnyForwardShadingEnabled(View.GetShaderPlatform()));
}

static bool IsSkylightActive(const FViewInfo& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;
	return Scene->SkyLight 
		&& Scene->SkyLight->ProcessedTexture
		&& View.Family->EngineShowFlags.SkyLighting;
}

bool ShouldRenderScreenSpaceAmbientOcclusion(const FViewInfo& View)
{
	bool bEnabled = true;

	if (!IsLpvIndirectPassRequired(View))
	{
		bEnabled = View.FinalPostProcessSettings.AmbientOcclusionIntensity > 0
			&& View.Family->EngineShowFlags.Lighting
			&& View.FinalPostProcessSettings.AmbientOcclusionRadius >= 0.1f
			&& !View.Family->UseDebugViewPS()
			&& (FSSAOHelper::IsBasePassAmbientOcclusionRequired(View) || IsAmbientCubemapPassRequired(View) || IsReflectionEnvironmentActive(View) || IsSkylightActive(View) || View.Family->EngineShowFlags.VisualizeBuffer)
			&& !IsSimpleForwardShadingEnabled(View.GetShaderPlatform());
	}
#if RHI_RAYTRACING
	bEnabled &= !ShouldRenderRayTracingAmbientOcclusion(View);
#endif
	return bEnabled;
}

// Async Passes of the GTAO.
// This can either just be the Horizon search if GBuffer Normals are needed or it can be
// Combined Horizon search and Integrate followed by the Spatial filter if no normals are needed
static FRenderingCompositeOutputRef AddPostProcessingGTAOAsyncPasses(FRHICommandListImmediate& RHICmdList, FPostprocessContext& Context, EGTAOType GTAOType)
{
	check((GTAOType == EGTAOType::EAsyncHorizonSearch) || (GTAOType == EGTAOType::EAsyncCombinedSpatial));

	const int32 ShaderQuality = FSSAOHelper::GetAmbientOcclusionShaderLevel(Context.View);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
	uint32 DownsampleFactor = CVarGTAODownsample.GetValueOnRenderThread() > 0 ? 2 : 1;

	FIntPoint BufferSize	 = SceneContext.GetBufferSizeXY();
	FIntPoint HorizonBufferSize = FIntPoint::DivideAndRoundUp(BufferSize, DownsampleFactor);

	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(HorizonBufferSize, PF_R32_FLOAT, FClearValueBinding::White, TexCreate_None, TexCreate_RenderTargetable, false));
	if (SceneContext.GetCurrentFeatureLevel() >= ERHIFeatureLevel::SM5)
	{
		Desc.TargetableFlags |= TexCreate_UAV;
	}
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneContext.ScreenSpaceGTAODepths, TEXT("ScreenSpaceGTAODepths"));

	Desc.Format = PF_R8G8;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneContext.ScreenSpaceGTAOHorizons, TEXT("ScreenSpaceGTAOHorizons"));

	const bool bSpatialPass = (CVarGTAOSpatialFilter.GetValueOnRenderThread() == 1);

	TRefCountPtr<IPooledRenderTarget> GTAODepthsTarget = SceneContext.ScreenSpaceGTAODepths;
	TRefCountPtr<IPooledRenderTarget> GTAOHorizonsTarget = SceneContext.ScreenSpaceGTAOHorizons;

	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(
		new (FMemStack::Get()) TRCPassForRDG<2, 3>(
			[GTAOType, DownsampleFactor, ShaderQuality, bSpatialPass, GTAODepthsTarget, GTAOHorizonsTarget](FRenderingCompositePass* InPass, FRenderingCompositePassContext& InContext)
	{
		FRDGBuilder GraphBuilder(InContext.RHICmdList);
		const FViewInfo& View = InContext.View;
		const FIntRect ViewRect = View.ViewRect;
		const FIntRect DownsampledViewRect = GetDownscaledRect(ViewRect, DownsampleFactor);

		FGTAOCommonParameters CommonParameters;
		CommonParameters.SceneTexturesUniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, View, ESceneTextureSetupMode::SceneDepth | ESceneTextureSetupMode::GBuffers);
		const FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, CommonParameters.SceneTexturesUniformBuffer);
		CommonParameters.SceneTexturesViewport = FScreenPassTextureViewport(SceneTextureParameters.SceneDepthTexture, ViewRect);

		CommonParameters.ShaderQuality = ShaderQuality;
		CommonParameters.DownscaleFactor = DownsampleFactor;
		CommonParameters.GTAOType = GTAOType;

		FScreenPassTexture HZBInput(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input0, TEXT("GTAOHZBInput")));
		FScreenPassTexture SceneDepth(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input1, TEXT("GTAOSceneDepth")), ViewRect);

		FScreenPassRenderTarget GTAOHorizons(GraphBuilder.RegisterExternalTexture(GTAOHorizonsTarget, TEXT("HorizonsOutput")), DownsampledViewRect, ERenderTargetLoadAction::ENoAction);
		FScreenPassRenderTarget GTAODepths(GraphBuilder.RegisterExternalTexture(GTAODepthsTarget, TEXT("DepthOutTexture")), DownsampledViewRect, ERenderTargetLoadAction::ENoAction);

		FGTAOHorizonSearchOutputs HorizonSearchOutputs;

		if (GTAOType == EGTAOType::EAsyncHorizonSearch)
		{
			HorizonSearchOutputs =
				AddGTAOHorizonSearchPass(
					GraphBuilder,
					View,
					CommonParameters,
					SceneDepth,
					HZBInput,
					GTAOHorizons,
					GTAODepths);

			InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, HorizonSearchOutputs.Color.Texture);
			InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output1, HorizonSearchOutputs.Depth.Texture);
		}
		else
		{
			check(GTAOType == EGTAOType::EAsyncCombinedSpatial);

			HorizonSearchOutputs =
				AddGTAOHorizonSearchIntegratePass(
					GraphBuilder,
					View,
					CommonParameters,
					SceneDepth,
					HZBInput,
					GTAODepths);

			if (bSpatialPass)
			{
				FScreenPassTexture SpatialOutput =
					AddGTAOSpatialFilter(
						GraphBuilder,
						View,
						CommonParameters,
						HorizonSearchOutputs.Color,
						HorizonSearchOutputs.Depth,
						GTAOHorizons);

				InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output2, SpatialOutput.Texture);
			}
		}

		InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, HorizonSearchOutputs.Color.Texture);
		InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output1, HorizonSearchOutputs.Depth.Texture);

		GraphBuilder.Execute();
	}));

	FRenderingCompositePass* HZBInput = Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessInput(const_cast<FViewInfo&>(Context.View).HZB));
	Pass->SetInput(ePId_Input0, HZBInput);

	Pass->SetInput(ePId_Input1, Context.SceneDepth);

	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
	return FRenderingCompositeOutputRef(Pass);
}

// The whole GTAO stack is run on the Gfx Pipe
static FRenderingCompositeOutputRef AddPostProcessingGTAOAllPasses(FRHICommandListImmediate& RHICmdList, FPostprocessContext& Context, EGTAOType GTAOType)
{
	const int32 ShaderQuality = FSSAOHelper::GetAmbientOcclusionShaderLevel(Context.View);
	uint32 DownsampleFactor = CVarGTAODownsample.GetValueOnRenderThread() > 0 ? 2 : 1;

	// This can run on async compute if available and we don't use the per pixel normals

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.bScreenSpaceAOIsValid = true;

	FSceneViewState* ViewState = Context.View.ViewState;

	const bool bSpatialPass = (CVarGTAOSpatialFilter.GetValueOnRenderThread() == 1);
	const bool bTemporalPass = (ViewState && CVarGTAOTemporalFilter.GetValueOnRenderThread() == 1);

	TRefCountPtr<IPooledRenderTarget> FinalAOTarget = SceneContext.ScreenSpaceAO;

	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(
		new (FMemStack::Get()) TRCPassForRDG<3, 1>(
			[GTAOType, DownsampleFactor, ShaderQuality, bSpatialPass, bTemporalPass, FinalAOTarget](FRenderingCompositePass* InPass, FRenderingCompositePassContext& InContext)
	{
		FRDGBuilder GraphBuilder(InContext.RHICmdList);
		const FViewInfo& View = InContext.View;
		const FIntRect ViewRect = View.ViewRect;
		const FIntRect DownsampledViewRect = GetDownscaledRect(ViewRect, DownsampleFactor);

		FGTAOCommonParameters CommonParameters;
		CommonParameters.SceneTexturesUniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, View, ESceneTextureSetupMode::SceneDepth | ESceneTextureSetupMode::GBuffers);
		const FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, CommonParameters.SceneTexturesUniformBuffer);
		CommonParameters.SceneTexturesViewport = FScreenPassTextureViewport(SceneTextureParameters.SceneDepthTexture, ViewRect);

		CommonParameters.ShaderQuality = ShaderQuality;
		CommonParameters.DownscaleFactor = DownsampleFactor;
		CommonParameters.GTAOType = GTAOType;

		FScreenPassTexture HZBInput(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input0, TEXT("GTAOHZBInput")));
		FScreenPassTexture SceneDepth(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input1, TEXT("GTAOSceneDepth")), ViewRect);
		FScreenPassTexture SceneVelocity(InPass->CreateRDGTextureForOptionalInput(GraphBuilder, ePId_Input2, TEXT("GTAOSceneVelocity")), ViewRect);

		FScreenPassRenderTarget FinalAO(GraphBuilder.RegisterExternalTexture(FinalAOTarget, TEXT("GTAOOutput")), ViewRect, ERenderTargetLoadAction::ELoad);

		FGTAOHorizonSearchOutputs HorizonSearchOutputs =
			AddGTAOHorizonSearchIntegratePass(
				GraphBuilder,
				View,
				CommonParameters,
				SceneDepth,
				HZBInput);

		FScreenPassTexture CurrentOutput = HorizonSearchOutputs.Color;
		if (bSpatialPass)
		{
			CurrentOutput =
				AddGTAOSpatialFilter(
					GraphBuilder,
					View,
					CommonParameters,
					CurrentOutput,
					HorizonSearchOutputs.Depth);
		}

		if (bTemporalPass)
		{
			const FGTAOTAAHistory& InputHistory = View.PrevViewInfo.GTAOHistory;
			FGTAOTAAHistory* OutputHistory = &View.ViewState->PrevFrameViewInfo.GTAOHistory;

			FScreenPassTextureViewport HistoryViewport(InputHistory.ReferenceBufferSize, InputHistory.ViewportRect);

			FScreenPassTexture HistoryColor;

			if (InputHistory.IsValid())
			{
				HistoryColor = FScreenPassTexture(GraphBuilder.RegisterExternalTexture(InputHistory.RT, TEXT("GTAOHistoryColor")), HistoryViewport.Rect);
			}
			else
			{
				HistoryColor = FScreenPassTexture(GraphBuilder.RegisterExternalTexture(GSystemTextures.WhiteDummy, TEXT("GTAODummyTexture")));
			}

			FGTAOTemporalOutputs TemporalOutputs =
				AddGTAOTemporalPass(
					GraphBuilder,
					InContext.View,
					CommonParameters,
					CurrentOutput,
					HorizonSearchOutputs.Depth,
					SceneVelocity,
					HistoryColor,
					HistoryViewport);

			OutputHistory->SafeRelease();
			GraphBuilder.QueueTextureExtraction(TemporalOutputs.OutputAO.Texture, &OutputHistory->RT);

			OutputHistory->ReferenceBufferSize = TemporalOutputs.TargetExtent;
			OutputHistory->ViewportRect = TemporalOutputs.ViewportRect;

			CurrentOutput = TemporalOutputs.OutputAO;
		}

		FScreenPassTexture FinalOutput = CurrentOutput;
		// TODO: Can't switch outputs since it's an external texture. Won't be a problem when we're fully over to RDG.
		//if (DownsampleFactor > 1)
		{
			FinalOutput =
				AddGTAOUpsamplePass(
					GraphBuilder,
					View,
					CommonParameters,
					CurrentOutput,
					SceneDepth,
					HorizonSearchOutputs.Depth,
					FinalAO);
		}

		InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, FinalOutput.Texture);

		GraphBuilder.Execute();
	}));

	FRenderingCompositePass* HZBInput = Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessInput(const_cast<FViewInfo&>(Context.View).HZB));
	Pass->SetInput(ePId_Input0, HZBInput);
	Pass->SetInput(ePId_Input1, Context.SceneDepth);
	Pass->AddDependency(Context.FinalOutput);

	if (bTemporalPass)
	{
		FRenderingCompositePass* SceneVelocity = Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessInput(SceneContext.SceneVelocity));
		Pass->SetInput(ePId_Input2, SceneVelocity);
	}

	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
	return FRenderingCompositeOutputRef(Pass);
}

// These are the passes run after Async where some are run before on the Async pipe
static FRenderingCompositeOutputRef AddPostProcessingGTAOPostAsync(FRHICommandListImmediate& RHICmdList, FPostprocessContext& Context, EGTAOType GTAOType)
{
	const int32 ShaderQuality = FSSAOHelper::GetAmbientOcclusionShaderLevel(Context.View);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.bScreenSpaceAOIsValid = true; 
	
	uint32 DownsampleFactor = CVarGTAODownsample.GetValueOnRenderThread() > 0 ? 2 : 1;
	FSceneViewState* ViewState = Context.View.ViewState;

	const bool bSpatialPass = (CVarGTAOSpatialFilter.GetValueOnRenderThread() == 1);
	const bool bTemporalPass = (ViewState && CVarGTAOTemporalFilter.GetValueOnRenderThread() == 1);

	TRefCountPtr<IPooledRenderTarget> FinalAOTarget = SceneContext.ScreenSpaceAO;

	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(
		new (FMemStack::Get()) TRCPassForRDG<4, 1>(
			[GTAOType, DownsampleFactor, ShaderQuality, bSpatialPass, bTemporalPass, FinalAOTarget](FRenderingCompositePass* InPass, FRenderingCompositePassContext& InContext)
	{
		FRDGBuilder GraphBuilder(InContext.RHICmdList);
		const FViewInfo& View = InContext.View;
		const FIntRect ViewRect = View.ViewRect;
		const FIntRect DownsampledViewRect = GetDownscaledRect(ViewRect, DownsampleFactor);

		FGTAOCommonParameters CommonParameters;
		CommonParameters.SceneTexturesUniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, View, ESceneTextureSetupMode::SceneDepth | ESceneTextureSetupMode::GBuffers);
		const FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, CommonParameters.SceneTexturesUniformBuffer);
		CommonParameters.SceneTexturesViewport = FScreenPassTextureViewport(SceneTextureParameters.SceneDepthTexture, ViewRect);

		CommonParameters.ShaderQuality = ShaderQuality;
		CommonParameters.DownscaleFactor = DownsampleFactor;
		CommonParameters.GTAOType = GTAOType;

		FScreenPassTexture SceneDepth(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input0, TEXT("GTAOSceneDepth")), ViewRect);
		FScreenPassTexture GTAOHorizons(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input1, TEXT("GTAOHorizons")), DownsampledViewRect);
		FScreenPassTexture GTAODepths(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input2, TEXT("GTAODepths")), DownsampledViewRect);

		FScreenPassRenderTarget FinalAO(GraphBuilder.RegisterExternalTexture(FinalAOTarget, TEXT("GTAOOutput")), ViewRect, ERenderTargetLoadAction::ENoAction);

		FScreenPassTexture CurrentOutput;

		if (GTAOType == EGTAOType::EAsyncHorizonSearch)
		{
			CurrentOutput =
				AddGTAOInnerIntegratePass(
					GraphBuilder,
					InContext.View,
					CommonParameters,
					SceneDepth,
					GTAOHorizons);

			if (bSpatialPass)
			{
				CurrentOutput =
					AddGTAOSpatialFilter(
						GraphBuilder,
						InContext.View,
						CommonParameters,
						CurrentOutput,
						GTAODepths);
			}
		}
		else
		{
			// If the Spatial Filter is running as part of the async then we'll render to the R channel of the horizons texture so it can be read in as part of the temporal
			CurrentOutput = GTAOHorizons;
		}

		if (bTemporalPass)
		{
			FScreenPassTexture SceneVelocity(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input3, TEXT("GTAOSceneVelocity")));

			const FGTAOTAAHistory& InputHistory = View.PrevViewInfo.GTAOHistory;
			FGTAOTAAHistory* OutputHistory = &View.ViewState->PrevFrameViewInfo.GTAOHistory;

			FScreenPassTextureViewport HistoryViewport(InputHistory.ReferenceBufferSize, InputHistory.ViewportRect);

			FScreenPassTexture HistoryColor;

			if (InputHistory.IsValid())
			{
				HistoryColor = FScreenPassTexture(GraphBuilder.RegisterExternalTexture(InputHistory.RT, TEXT("GTAOHistoryColor")), HistoryViewport.Rect);
			}
			else
			{
				HistoryColor = FScreenPassTexture(GraphBuilder.RegisterExternalTexture(GSystemTextures.WhiteDummy, TEXT("GTAODummyTexture")));
			}

			FGTAOTemporalOutputs TemporalOutputs =
				AddGTAOTemporalPass(
					GraphBuilder,
					InContext.View,
					CommonParameters,
					CurrentOutput,
					GTAODepths,
					SceneVelocity,
					HistoryColor,
					HistoryViewport);

			OutputHistory->SafeRelease();
			GraphBuilder.QueueTextureExtraction(TemporalOutputs.OutputAO.Texture, &OutputHistory->RT);

			OutputHistory->ReferenceBufferSize = TemporalOutputs.TargetExtent;
			OutputHistory->ViewportRect = TemporalOutputs.ViewportRect;

			CurrentOutput = TemporalOutputs.OutputAO;
		}

		FScreenPassTexture FinalOutput = CurrentOutput;

		// TODO: Can't switch outputs since it's an external texture. Won't be a problem when we're fully over to RDG.
		//if (DownsampleFactor > 1)
		{
			FinalOutput =
				AddGTAOUpsamplePass(
					GraphBuilder,
					View,
					CommonParameters,
					CurrentOutput,
					SceneDepth,
					GTAODepths,
					FinalAO);
		}

		InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, FinalOutput.Texture);

		GraphBuilder.Execute();
	}));

	Pass->SetInput(ePId_Input0, Context.SceneDepth);

	FRenderingCompositePass* GTAOHorizons = Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessInput(SceneContext.ScreenSpaceGTAOHorizons));
	Pass->SetInput(ePId_Input1, GTAOHorizons);

	FRenderingCompositePass* GTAODepths = Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessInput(SceneContext.ScreenSpaceGTAODepths));
	Pass->SetInput(ePId_Input2, GTAODepths);

	if (bTemporalPass)
	{
		FRenderingCompositePass* SceneVelocity = Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessInput(SceneContext.SceneVelocity));
		Pass->SetInput(ePId_Input3, SceneVelocity);
	}

	Pass->AddDependency(Context.FinalOutput);
	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
	return FRenderingCompositeOutputRef(Pass);
}

static ESSAOType GetDownscaleSSAOType(const FViewInfo& View)
{
	return FSSAOHelper::IsAmbientOcclusionCompute(View) ? ESSAOType::ECS : ESSAOType::EPS;
}

static ESSAOType GetFullscreenSSAOType(const FViewInfo& View, uint32 Levels)
{
	if (FSSAOHelper::IsAmbientOcclusionCompute(View))
	{
		if (FSSAOHelper::IsAmbientOcclusionAsyncCompute(View, Levels))
		{
			return ESSAOType::EAsyncCS;
		}

		return ESSAOType::ECS;
	}

	return ESSAOType::EPS;
}

// @param Levels 0..3, how many different resolution levels we want to render
static FRenderingCompositeOutputRef AddPostProcessingAmbientOcclusion(FRHICommandListImmediate& RHICmdList, FPostprocessContext& Context, uint32 Levels)
{
	check(Levels >= 0 && Levels <= 3);

	const int32 ShaderQuality = FSSAOHelper::GetAmbientOcclusionShaderLevel(Context.View);
	const ESSAOType DownscaleSSAOType = GetDownscaleSSAOType(Context.View);
	const ESSAOType FullscreenSSAOType = GetFullscreenSSAOType(Context.View, Levels);

	// If there is no temporal upsampling, we need a smooth pass to get rid of the grid pattern.
	// Pixel shader version has relatively smooth result so no need to do extra work.
	const bool bNeedSmoothingPass = FullscreenSSAOType != ESSAOType::EPS && Context.View.AntiAliasingMethod != AAM_TemporalAA && CVarSSAOSmoothPass.GetValueOnRenderThread();

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const TRefCountPtr<IPooledRenderTarget> OutputTarget = SceneContext.ScreenSpaceAO;

	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(
		new (FMemStack::Get()) TRCPassForRDG<3, 1>(
			[Levels, ShaderQuality, DownscaleSSAOType, FullscreenSSAOType, bNeedSmoothingPass, OutputTarget](FRenderingCompositePass* InPass, FRenderingCompositePassContext& InContext)
	{
		FRDGBuilder GraphBuilder(InContext.RHICmdList);
		const FViewInfo& View = InContext.View;
		const FIntRect ViewRect = View.ViewRect;

		SSAOCommonParameters CommonParameters;

		CommonParameters.SceneTexturesUniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, View, ESceneTextureSetupMode::SceneDepth | ESceneTextureSetupMode::GBuffers);
		const FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, CommonParameters.SceneTexturesUniformBuffer);
		CommonParameters.SceneTexturesViewport = FScreenPassTextureViewport(SceneTextureParameters.SceneDepthTexture, ViewRect);

		CommonParameters.ShaderQuality = ShaderQuality;
		CommonParameters.DownscaleType = DownscaleSSAOType;
		CommonParameters.FullscreenType = FullscreenSSAOType;
		CommonParameters.bNeedSmoothingPass = bNeedSmoothingPass;

		FScreenPassTexture HZBInput(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input0, TEXT("AmbientOcclusionHZBInput")));
		FScreenPassTexture GBufferA(InPass->CreateRDGTextureForOptionalInput(GraphBuilder, ePId_Input2, TEXT("AmbientOcclusionGBufferA")), ViewRect);

		FScreenPassRenderTarget FinalTarget = FScreenPassRenderTarget(GraphBuilder.RegisterExternalTexture(OutputTarget, TEXT("AmbientOcclusionDirect")), ViewRect, ERenderTargetLoadAction::ENoAction);

		FScreenPassTexture AmbientOcclusionInMip1;
		FScreenPassTexture AmbientOcclusionPassMip1;
		if (Levels >= 2)
		{
			FScreenPassTexture SceneDepth(InPass->CreateRDGTextureForRequiredInput(GraphBuilder, ePId_Input1, TEXT("AmbientOcclusionSetupInput")), ViewRect);
			AmbientOcclusionInMip1 =
				AddAmbientOcclusionSetupPass(
					GraphBuilder,
					View,
					CommonParameters,
					SceneDepth);

			FScreenPassTexture AmbientOcclusionPassMip2;
			if (Levels >= 3)
			{
				FScreenPassTexture AmbientOcclusionInMip2 =
					AddAmbientOcclusionSetupPass(
						GraphBuilder,
						View,
						CommonParameters,
						AmbientOcclusionInMip1);

				AmbientOcclusionPassMip2 =
					AddAmbientOcclusionStepPass(
						GraphBuilder,
						View,
						CommonParameters,
						AmbientOcclusionInMip2,
						AmbientOcclusionInMip2,
						FScreenPassTexture(),
						HZBInput);
			}

			AmbientOcclusionPassMip1 =
				AddAmbientOcclusionStepPass(
					GraphBuilder,
					View,
					CommonParameters,
					AmbientOcclusionInMip1,
					AmbientOcclusionInMip1,
					AmbientOcclusionPassMip2,
					HZBInput);
		}

		FScreenPassTexture FinalOutput =
			AddAmbientOcclusionFinalPass(
				GraphBuilder,
				View,
				CommonParameters,
				GBufferA,
				AmbientOcclusionInMip1,
				AmbientOcclusionPassMip1,
				HZBInput,
				FinalTarget);

		InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, FinalOutput.Texture);

		GraphBuilder.Execute();
	}));

	FRenderingCompositePass* HZBInput = Context.Graph.RegisterPass(new FRCPassPostProcessInput(const_cast<FViewInfo&>(Context.View).HZB));
	Pass->SetInput(ePId_Input0, HZBInput);

	if (Levels >= 2)
	{
		Pass->SetInput(ePId_Input1, Context.SceneDepth);
	}

	if (SceneContext.GBufferA)
	{
		FRenderingCompositePass* GBufferA = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.GBufferA));
		Pass->SetInput(ePId_Input2, GBufferA);
	}

	Pass->AddDependency(Context.FinalOutput);
	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);

	SceneContext.bScreenSpaceAOIsValid = true;

	return FRenderingCompositeOutputRef(Pass);
}

void FCompositionLighting::ProcessBeforeBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View, bool bDBuffer, uint32 SSAOLevels)
{
	check(IsInRenderingThread());

	// so that the passes can register themselves to the graph
	if (bDBuffer || SSAOLevels)
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

		// Add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ----------

		// decals are before AmbientOcclusion so the decal can output a normal that AO is affected by
		if (bDBuffer) 
		{
			Context.FinalOutput = AddDeferredDecalPass(Context.Graph, Context.FinalOutput, DRS_BeforeBasePass);
		}

		if (SSAOLevels && FSSAOHelper::GetGTAOPassType(View, SSAOLevels) != EGTAOType::ENonAsync)
		{
			AddPostProcessingAmbientOcclusion(RHICmdList, Context, SSAOLevels);
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, CompositionBeforeBasePass);
		SCOPED_GPU_STAT(RHICmdList, CompositionBeforeBasePass);

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("Composition_BeforeBasePass"));
	}
}

void FCompositionLighting::ProcessAfterBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	// might get renamed to refracted or ...WithAO
	SceneContext.GetSceneColor()->SetDebugName(TEXT("SceneColor"));
	// to be able to observe results with VisualizeTexture

	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GetSceneColor());
	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferA);
	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferB);
	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferC);
	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferD);
	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferE);
	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferF);
	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.SceneVelocity);
	GVisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.ScreenSpaceAO);
	
	// so that the passes can register themselves to the graph
	if(CanOverlayRayTracingOutput(View))
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

		// Add the passes we want to add to the graph ----------
		
		if( Context.View.Family->EngineShowFlags.Decals &&
			!Context.View.Family->EngineShowFlags.ShaderComplexity)
		{
			Context.FinalOutput = AddDeferredDecalPass(Context.Graph, Context.FinalOutput, DRS_AfterBasePass);
		}

		// decal are distracting when looking at LightCulling.
		bool bDoDecal = Context.View.Family->EngineShowFlags.Decals && !Context.View.Family->EngineShowFlags.VisualizeLightCulling;

		if (bDoDecal && IsUsingGBuffers(View.GetShaderPlatform()))
		{
			// decals are before AmbientOcclusion so the decal can output a normal that AO is affected by
			Context.FinalOutput = AddDeferredDecalPass(Context.Graph, Context.FinalOutput, DRS_BeforeLighting);
		}

		if (bDoDecal && !IsSimpleForwardShadingEnabled(View.GetShaderPlatform()))
		{
			// DBuffer decals with emissive component
			Context.FinalOutput = AddDeferredDecalPass(Context.Graph, Context.FinalOutput, DRS_Emissive);
		}

		// Forwared shading SSAO is applied before the basepass using only the depth buffer.
		if (!IsForwardShadingEnabled(View.GetShaderPlatform()))
		{
			FRenderingCompositeOutputRef AmbientOcclusion;
#if RHI_RAYTRACING
			if (ShouldRenderRayTracingAmbientOcclusion(View) && SceneContext.bScreenSpaceAOIsValid)
			{
				AmbientOcclusion = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.ScreenSpaceAO));
			}
#endif
			uint32 SSAOLevels = FSSAOHelper::ComputeAmbientOcclusionPassCount(Context.View);
			if (SSAOLevels)
			{
				const EGTAOType GTAOType = FSSAOHelper::GetGTAOPassType(View, SSAOLevels);

				// If doing the Split GTAO method then we need to do the second part here.
				if (GTAOType == EGTAOType::EAsyncHorizonSearch || GTAOType == EGTAOType::EAsyncCombinedSpatial)
				{
					AmbientOcclusion = AddPostProcessingGTAOPostAsync(RHICmdList, Context, GTAOType);

					ensureMsgf(
						FDecalRendering::BuildVisibleDecalList(*(FScene*)Context.View.Family->Scene, Context.View, DRS_AmbientOcclusion, nullptr) == false,
						TEXT("Ambient occlusion decals are not supported with Async compute SSAO."));
				}
				else
				{
					if (GTAOType == EGTAOType::ENonAsync)
					{
						AmbientOcclusion = AddPostProcessingGTAOAllPasses(RHICmdList, Context, EGTAOType::ENonAsync);
					}
					else
					{
						// SSAO
						AmbientOcclusion = AddPostProcessingAmbientOcclusion(RHICmdList, Context, SSAOLevels);
					}

					if (bDoDecal)
					{
						Context.FinalOutput = AddDeferredDecalPass(Context.Graph, Context.FinalOutput, DRS_AmbientOcclusion);
					}
				}
			}
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, LightCompositionTasks_PreLighting);
		SCOPED_GPU_STAT(RHICmdList, CompositionPreLighting);

		TRefCountPtr<IPooledRenderTarget>& SceneColor = SceneContext.GetSceneColor();

		Context.FinalOutput.GetOutput()->RenderTargetDesc = SceneColor->GetDesc();
		Context.FinalOutput.GetOutput()->PooledRenderTarget = SceneColor;

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("CompositionLighting_AfterBasePass"));
	}

	SceneContext.ScreenSpaceGTAOHorizons.SafeRelease();
	SceneContext.ScreenSpaceGTAODepths.SafeRelease();
}


void FCompositionLighting::ProcessLpvIndirect(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	
	FMemMark Mark(FMemStack::Get());
	FRenderingCompositePassContext CompositeContext(RHICmdList, View);
	FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		FRenderingCompositePass* SSAO = Context.Graph.RegisterPass(new FRCPassPostProcessInput(SceneContext.ScreenSpaceAO));

		FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new FRCPassPostProcessLpvIndirect());
		Pass->SetInput(ePId_Input0, Context.FinalOutput);
		Pass->SetInput(ePId_Input1, SSAO );

		Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
	}

	// The graph setup should be finished before this line ----------------------------------------

	SCOPED_DRAW_EVENT(RHICmdList, CompositionLpvIndirect);
	SCOPED_GPU_STAT(RHICmdList, CompositionLpvIndirect);

	// we don't replace the final element with the scenecolor because this is what those passes should do by themself

	CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("CompositionLighting"));
}

bool FCompositionLighting::CanProcessAsyncSSAO(const TArray<FViewInfo>& Views)
{
	bool bAnyAsyncSSAO = true;
	for (int32 i = 0; i < Views.Num(); ++i)
	{
		uint32 Levels = FSSAOHelper::ComputeAmbientOcclusionPassCount(Views[i]);
		if (!FSSAOHelper::IsAmbientOcclusionAsyncCompute(Views[i], Levels))
		{
			bAnyAsyncSSAO = false;
			break;
		}
	}
	return bAnyAsyncSSAO;
}

void FCompositionLighting::PrepareAsyncSSAO(FRHICommandListImmediate& RHICmdList)
{
	//clear out last frame's fence.
	ensureMsgf(AsyncSSAOTransition == nullptr, TEXT("Old AsyncCompute SSAO transition has not been cleared."));

	// Setup 'invalid' transition because the resource isn't know - this needs to be fixed!
	FRHITransitionInfo SRVTransition((FRHIUnorderedAccessView*)nullptr, ERHIAccess::Unknown, ERHIAccess::SRVMask);
	AsyncSSAOTransition = RHICreateTransition(ERHIPipeline::AsyncCompute, ERHIPipeline::Graphics, ERHICreateTransitionFlags::None, MakeArrayView(&SRVTransition, 1));

	//Grab the async compute commandlist.
	FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
	RHICmdListComputeImmediate.SetAsyncComputeBudget(FSSAOHelper::GetAmbientOcclusionAsyncComputeBudget());
}

void FCompositionLighting::ProcessAsyncSSAO(FRHICommandListImmediate& RHICmdList, const TArray<FViewInfo>& Views)
{
	check(IsInRenderingThread());
	{
		PrepareAsyncSSAO(RHICmdList);

		// so that the passes can register themselves to the graph
		for (int32 i = 0; i < Views.Num(); ++i)
		{
			const FViewInfo& View = Views[i];

			const uint32 Levels = FSSAOHelper::ComputeAmbientOcclusionPassCount(View);
			const EGTAOType GTAOType = FSSAOHelper::GetGTAOPassType(View, Levels);

			if (GTAOType == EGTAOType::EAsyncCombinedSpatial || GTAOType == EGTAOType::EAsyncHorizonSearch)
			{
				FMemMark Mark(FMemStack::Get());
				FRenderingCompositePassContext CompositeContext(RHICmdList, View);

				// Add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph)
				SCOPED_GPU_MASK(RHICmdList, View.GPUMask);
				SCOPED_GPU_MASK(FRHICommandListExecutor::GetImmediateAsyncComputeCommandList(), View.GPUMask);

				FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

				FRenderingCompositeOutputRef AmbientOcclusion = AddPostProcessingGTAOAsyncPasses(RHICmdList, Context, GTAOType);
				Context.FinalOutput = FRenderingCompositeOutputRef(AmbientOcclusion);

				// The graph setup should be finished before this line ----------------------------------------
				CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("Composition_ProcessAsyncSSAO"));
			}
		}
		FinishAsyncSSAO(RHICmdList);
	}
}

void FCompositionLighting::FinishAsyncSSAO(FRHICommandListImmediate& RHICmdList)
{
	if (AsyncSSAOTransition)
	{
		//Grab the async compute commandlist.
		FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();

		RHICmdListComputeImmediate.SetAsyncComputeBudget(EAsyncComputeBudget::EAll_4);
		RHICmdListComputeImmediate.BeginTransition(AsyncSSAOTransition);
		FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);		
	}
}

void FCompositionLighting::GfxWaitForAsyncSSAO(FRHICommandListImmediate& RHICmdList)
{
	if (AsyncSSAOTransition)
	{
		RHICmdList.EndTransition(AsyncSSAOTransition);
		AsyncSSAOTransition = nullptr;
	}
}