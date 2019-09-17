// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "ScreenPass.h"
#include "OverridePassSequence.h"
#include "PostProcess/RenderingCompositionGraph.h"

struct FSelectionOutlineInputs : public FOverridePassInputs
{
	FRDGTextureViewRectPair SceneColor;
	FRDGTextureViewRectPair SceneDepth;
};

using FSelectionOutlineOutputs = FRDGTextureViewRectPair;

FSelectionOutlineOutputs AddSelectionOutlinePass(FRDGBuilder& GraphBuilder, const FScreenPassViewInfo& ScreenPassView, const FSelectionOutlineInputs& Inputs);

FRenderingCompositeOutputRef AddSelectionOutlinePass(FRenderingCompositionGraph& Graph, FRenderingCompositeOutputRef Input);

#endif
