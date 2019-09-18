// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

FScreenPassTexture AddBloomSetupPass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FScreenPassTexture SceneColor,
	FRDGTextureRef EyeAdaptationTexture,
	float BloomThreshold);