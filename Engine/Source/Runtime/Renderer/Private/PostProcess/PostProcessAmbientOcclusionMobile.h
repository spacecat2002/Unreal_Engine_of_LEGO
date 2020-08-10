// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbientOcclusionMobile.h
=============================================================================*/

#pragma once
#include "CoreMinimal.h"
#include "RendererInterface.h"

struct FAmbientOcclusionMobileOutputs
{
	TRefCountPtr<IPooledRenderTarget> AmbientOcclusionTexture;
	TRefCountPtr<IPooledRenderTarget> SceneDepthZ;

	bool IsValid()
	{
		return AmbientOcclusionTexture.IsValid();
	}

	void Release()
	{
		AmbientOcclusionTexture.SafeRelease();
		SceneDepthZ.SafeRelease();
	}
};

extern FAmbientOcclusionMobileOutputs GAmbientOcclusionMobileOutputs;