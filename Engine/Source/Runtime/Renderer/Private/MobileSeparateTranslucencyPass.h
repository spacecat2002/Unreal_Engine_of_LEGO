// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FRCSeparateTranslucensyPassMobile : public TRenderingCompositePassBase<1, 1>
{
public:
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
	virtual FRenderingCompositeOutput* GetOutput(EPassOutputId InPassOutputId) override;
	virtual const TCHAR* GetDebugName() { return TEXT("FRCSeparateTranslucensyPassMobile"); }
};

// Returns whether separate translucency is enabled and there primitives to draw in the view
bool IsMobileSeparateTranslucencyActive(const FViewInfo& View);