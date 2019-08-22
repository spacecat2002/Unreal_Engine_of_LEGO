// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBloomSetup.h: Post processing bloom threshold pass implementation.
=============================================================================*/

#pragma once

#include "ScreenPass.h"
#include "PostProcess/RenderingCompositionGraph.h"

FRenderingCompositeOutputRef AddBloomSetupPass(
	FRenderingCompositionGraph& Graph,
	FRenderingCompositeOutputRef SceneColor,
	FRenderingCompositeOutputRef EyeAdaptation);