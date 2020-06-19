// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbientOcclusionMobile.h
=============================================================================*/

#pragma once
#include "../../../RenderCore/Public/RendererInterface.h"

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