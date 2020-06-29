// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsRendering.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"

void AddServiceLocalQueuePass(FRDGBuilder& GraphBuilder);

void RenderHairPrePass(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	TArray<FViewInfo>& Views,
	FHairStrandClusterData HairClusterData,
	FHairStrandsDatas& OutHairDatas)
{
	// #hair_todo: Add multi-view
	const bool bIsViewCompatible = Views.Num() > 0 && Views[0].Family->ViewMode == VMI_Lit;
	if (IsHairStrandsEnable(Scene->GetShaderPlatform()) && bIsViewCompatible)
	{
		const ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();

		//SCOPED_GPU_STAT(RHICmdList, HairRendering);
		OutHairDatas.MacroGroupsPerViews = CreateHairStrandsMacroGroups(GraphBuilder, Scene, Views);

		// Culling/LOD pass for DOM and Voxelisation altogether
		FHairCullingParams CullingParams;
		CullingParams.bShadowViewMode = true;
		CullingParams.bCullingProcessSkipped = false;
		ComputeHairStrandsClustersCulling(GraphBuilder, *GetGlobalShaderMap(FeatureLevel), Views, CullingParams, HairClusterData);

		AddServiceLocalQueuePass(GraphBuilder);

		// Voxelization and Deep Opacity Maps
		VoxelizeHairStrands(GraphBuilder, Scene, Views, OutHairDatas.MacroGroupsPerViews);
		RenderHairStrandsDeepShadows(GraphBuilder, Scene, Views, OutHairDatas.MacroGroupsPerViews);

		AddServiceLocalQueuePass(GraphBuilder);
	}
}

void RenderHairBasePass(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	FSceneRenderTargets& SceneContext,
	TArray<FViewInfo>& Views,
	FHairStrandClusterData HairClusterData,
	FHairStrandsDatas& OutHairDatas)
{
	// #hair_todo: Add multi-view
	const bool bIsViewCompatible = Views.Num() > 0 && Views[0].Family->ViewMode == VMI_Lit;
	if (IsHairStrandsEnable(Scene->GetShaderPlatform()) && bIsViewCompatible)
	{
		const ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();

		// Culling/LOD pass for visibility (must be done after HZB is generated)
		FHairCullingParams CullingParams;
		CullingParams.bCullingProcessSkipped = false;
		CullingParams.bShadowViewMode = false;
		ComputeHairStrandsClustersCulling(GraphBuilder, *GetGlobalShaderMap(FeatureLevel), Views, CullingParams, HairClusterData);

		// Hair visibility pass
		TRefCountPtr<IPooledRenderTarget> SceneColor = SceneContext.IsSceneColorAllocated() ? SceneContext.GetSceneColor() : nullptr;
		OutHairDatas.HairVisibilityViews = RenderHairStrandsVisibilityBuffer(GraphBuilder, Scene, Views, SceneContext.GBufferB, SceneColor, SceneContext.SceneDepthZ, SceneContext.SceneVelocity, OutHairDatas.MacroGroupsPerViews);

		// Reset indirect draw buffer
		ResetHairStrandsClusterToLOD0(GraphBuilder, *GetGlobalShaderMap(FeatureLevel), HairClusterData);

		if (SceneContext.bScreenSpaceAOIsValid && SceneContext.ScreenSpaceAO)
		{
			RenderHairStrandsAmbientOcclusion(
				GraphBuilder,
				Views,
				&OutHairDatas,
				SceneContext.ScreenSpaceAO);
		}
	}
}
