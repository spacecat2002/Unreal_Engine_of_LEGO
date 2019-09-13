// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "ScreenPass.h"
#include "PostProcess/RenderingCompositionGraph.h"

FRDGTextureRef AddSelectionOutlinePass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureRef SceneColorTexture,
	FIntRect SceneColorViewRect,
	FRDGTextureRef SceneDepthTexture,
	FIntRect SceneDepthViewRect);

FRenderingCompositeOutputRef AddSelectionOutlinePass(FRenderingCompositionGraph& Graph, FRenderingCompositeOutputRef Input);

#endif
