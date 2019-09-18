// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"

struct FVisualizeDOFInputs : public FOverridePassInputs
{
	FScreenPassTexture SceneColor;
	FScreenPassTexture SceneDepth;
};

using FVisualizeDOFOutputs = FScreenPassTexture;

FVisualizeDOFOutputs AddVisualizeDOFPass(FRDGBuilder& GraphBuilder, const FScreenPassViewInfo& ScreenPassView, const FVisualizeDOFInputs& Inputs);