// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FRCDistortionAccumulatePassMobile : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCDistortionAccumulatePassMobile(FIntPoint InPrePostSourceViewportSize, FScene* InScene) 
		: PrePostSourceViewportSize(InPrePostSourceViewportSize) 
		, Scene(InScene)
	{ }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
	virtual const TCHAR* GetDebugName() { return TEXT("FRCDistortionAccumulatePassMobile"); }
private:
	FIntPoint PrePostSourceViewportSize;
	FScene* Scene; 
};

class FRCDistortionMergePassMobile : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCDistortionMergePassMobile(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
	virtual const TCHAR* GetDebugName() { return TEXT("FRCDistortionMergePassMobile"); }
private:
	FIntPoint PrePostSourceViewportSize;
};

// Returns whether distortion is enabled and there primitives to draw
bool IsMobileDistortionActive(const FViewInfo& View);
