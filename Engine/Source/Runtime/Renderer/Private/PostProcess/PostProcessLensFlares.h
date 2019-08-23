// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PostProcess/RenderingCompositionGraph.h"
#include "ScreenPass.h"

FRenderingCompositeOutputRef AddLensFlarePass(
	FPostprocessContext& Context,
	FRenderingCompositeOutputRef BloomInput,
	TArrayView<FRenderingCompositeOutputRef> SceneColorDownsampleChain,
	bool bCompositeWithBloom);