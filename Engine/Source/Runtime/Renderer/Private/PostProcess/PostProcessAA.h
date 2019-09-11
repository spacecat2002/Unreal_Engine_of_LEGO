// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

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

FRDGTextureRef AddFXAAPass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureRef SceneColorTexture,
	FIntRect SceneColorViewRect,
	EFXAAQuality Quality);