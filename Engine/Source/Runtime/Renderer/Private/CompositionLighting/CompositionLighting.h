// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CompositionLighting.h: The center for all deferred lighting activities.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SceneRendering.h"

class FPersistentUniformBuffers;

/**
 * The center for all screen space processing activities (e.g. G-buffer manipulation, lighting).
 */
class FCompositionLighting
{
public:
	void ProcessBeforeBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View, bool bDBuffer, uint32 SSAOLevels);

	void ProcessAfterBasePass(
		FRDGBuilder& GraphBuilder,
		FPersistentUniformBuffers& Scene,
		const FViewInfo& View,
		TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTexturesUniformBuffer);

	// only call if LPV is enabled
	void ProcessLpvIndirect(FRHICommandListImmediate& RHICmdList, FViewInfo& View);

	bool CanProcessAsyncSSAO(const TArray<FViewInfo>& Views);
	void ProcessAsyncSSAO(FRHICommandListImmediate& RHICmdList, const TArray<FViewInfo>& Views);
	void GfxWaitForAsyncSSAO(FRDGBuilder& GraphBuilder);

private:
	void PrepareAsyncSSAO(FRHICommandListImmediate& RHICmdList);
	void FinishAsyncSSAO(FRHICommandListImmediate& RHICmdList);
	const FRHITransition* AsyncSSAOTransition = nullptr;
};

/** The global used for deferred lighting. */
extern FCompositionLighting GCompositionLighting;

extern bool ShouldRenderScreenSpaceAmbientOcclusion(const FViewInfo& View);