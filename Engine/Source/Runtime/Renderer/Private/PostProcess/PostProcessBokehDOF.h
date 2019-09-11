// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

FRDGTextureRef AddVisualizeDOFPass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureRef ColorTexture,
	FRDGTextureRef DepthTexture,
	FIntRect ViewRect);