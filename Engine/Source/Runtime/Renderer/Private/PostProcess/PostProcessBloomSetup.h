// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

FRDGTextureViewRectPair AddBloomSetupPass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureViewRectPair SceneColor,
	FRDGTextureRef EyeAdaptationTexture,
	float BloomThreshold);