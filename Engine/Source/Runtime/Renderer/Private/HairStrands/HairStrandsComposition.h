// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairStrandsComposition.h: Hair strands pixel composition implementation.
=============================================================================*/

#pragma once

#include "RenderGraph.h"

void RenderHairComposition(
	FRDGBuilder& GraphBuilder, 
	const TArray<FViewInfo>& Views,
	const struct FHairStrandsDatas* HairDatas,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef SceneDepthTexture);
