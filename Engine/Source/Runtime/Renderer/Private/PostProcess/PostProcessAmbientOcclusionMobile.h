// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbientOcclusionMobile.h
=============================================================================*/

#pragma once

struct FAmbientOcclusionMobileOutputs
{
	TRefCountPtr<IPooledRenderTarget> AmbientOcclusionTexture;

	bool IsValid()
	{
		return AmbientOcclusionTexture.IsValid();
	}

	void Release()
	{
		AmbientOcclusionTexture.SafeRelease();
	}
};