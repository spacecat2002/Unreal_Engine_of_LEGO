// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

struct FStreamingAccuracyLegendInputs
{
	FScreenPassRenderTarget OverrideOutput;
	FScreenPassTexture SceneColor;
	TArrayView<const FLinearColor> Colors;
};

FScreenPassTexture AddStreamingAccuracyLegendPass(FRDGBuilder& GraphBuilder, const FScreenPassViewInfo& ScreenPassView, const FStreamingAccuracyLegendInputs& Inputs);