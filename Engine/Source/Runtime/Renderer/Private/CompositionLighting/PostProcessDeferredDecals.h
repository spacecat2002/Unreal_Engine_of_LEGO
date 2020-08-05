// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraph.h"
#include "DecalRenderingCommon.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FViewInfo;

inline bool IsWritingToGBufferA(FDecalRenderingCommon::ERenderTargetMode RenderTargetMode)
{
	return RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferWithNormal
		|| RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteWithNormal
		|| RenderTargetMode == FDecalRenderingCommon::RTM_GBufferNormal;
}

inline bool IsWritingToDepth(FDecalRenderingCommon::ERenderTargetMode RenderTargetMode)
{
	return RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteWithNormal
		|| RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteNoNormal;
}

struct FDeferredDecalPassTextures
{
	// Scene textures of various configurations used by decal passes.
	TRDGUniformBufferRef<FSceneTextureUniformParameters> DepthUniformBuffer = nullptr;
	TRDGUniformBufferRef<FSceneTextureUniformParameters> GBufferAUniformBuffer = nullptr;
	TRDGUniformBufferRef<FSceneTextureUniformParameters> DepthAndGBufferAUniformBuffer = nullptr;

	// Potential render targets for the decal pass.
	FRDGTextureMSAA Depth;
	FRDGTextureRef Color = nullptr;
	FRDGTextureRef ScreenSpaceAO = nullptr;
	FRDGTextureRef GBufferA = nullptr;
	FRDGTextureRef GBufferB = nullptr;
	FRDGTextureRef GBufferC = nullptr;
	FRDGTextureRef GBufferE = nullptr;

	// [Input / Output]: D-Buffer targets allocated on-demand for the D-Buffer pass.
	FRDGTextureRef DBufferA = nullptr;
	FRDGTextureRef DBufferB = nullptr;
	FRDGTextureRef DBufferC = nullptr;
	FRDGTextureRef DBufferMask = nullptr;
};

FDeferredDecalPassTextures GetDeferredDecalPassTextures(FRDGBuilder& GraphBuilder);

void AddDeferredDecalPass(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FDeferredDecalPassTextures& Textures, EDecalRenderStage RenderStage);

FRenderingCompositeOutputRef AddDeferredDecalPass(
	FRenderingCompositionGraph& Graph,
	FRenderingCompositeOutputRef Input,
	EDecalRenderStage DecalRenderStage);

BEGIN_SHADER_PARAMETER_STRUCT(FDeferredDecalPassParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void GetDeferredDecalPassParameters(
	const FViewInfo& View,
	const FDeferredDecalPassTextures& DecalPassTextures,
	ERenderTargetLoadAction DBufferLoadAction,
	FDecalRenderingCommon::ERenderTargetMode RenderTargetMode,
	FDeferredDecalPassParameters& PassParameters);

void RenderMeshDecals(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FDeferredDecalPassTextures& DecalPassTextures,
	ERenderTargetLoadAction DBufferLoadAction,
	EDecalRenderStage DecalRenderStage);