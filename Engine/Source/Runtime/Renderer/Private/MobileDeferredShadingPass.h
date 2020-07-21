// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandListImmediate;
class FScene;
class FViewInfo;

void MobileDeferredShadingPass(FRHICommandListImmediate& RHICmdList, const FScene& Scene, const FViewInfo& View);