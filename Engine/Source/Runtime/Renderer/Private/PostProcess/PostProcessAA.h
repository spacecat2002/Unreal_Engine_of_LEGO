// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"

enum class EFXAAQuality : uint32
{
	// Lowest Quality / Fastest
	Q0,
	Q1,
	Q2,
	Q3,
	Q4,
	Q5,
	// Highest Quality / Slowest
	MAX
};

EFXAAQuality GetFXAAQuality();

struct FFXAAInputs : public FOverridePassInputs
{
	FRDGTextureViewRectPair SceneColor;

	EFXAAQuality Quality = EFXAAQuality::MAX;
};

using FFXAAOutputs = FRDGTextureViewRectPair;

FFXAAOutputs AddFXAAPass(FRDGBuilder& GraphBuilder, const FScreenPassViewInfo& ScreenPassView, const FFXAAInputs& Inputs);