// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.h: Post process MotionBlur implementation.
=============================================================================*/

#pragma once

#include "ScreenPass.h"

FRDGTextureRef ComputeMitchellNetravaliDownsample(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureViewRectPair Input,
	const FScreenPassTextureViewport OutputViewport);