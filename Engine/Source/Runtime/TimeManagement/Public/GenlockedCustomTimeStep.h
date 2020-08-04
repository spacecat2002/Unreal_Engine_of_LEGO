// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FixedFrameRateCustomTimeStep.h"
#include "GenlockedCustomTimeStep.generated.h"

/**
 * Class to control the Engine Timestep from a Genlock signal.
 */
UCLASS(Abstract)
class TIMEMANAGEMENT_API UGenlockedCustomTimeStep : public UFixedFrameRateCustomTimeStep
{
	GENERATED_BODY()

public:
	//~ UFixedFrameRateCustomTimeStep interface
	virtual FFrameRate GetFixedFrameRate() const PURE_VIRTUAL(UGenlockedCustomTimeStep::GetFixedFrameRate, return FFrameRate(24,1););

	/** Get the sync rate (not always the same as the fixed frame rate) */
	virtual FFrameRate GetSyncRate() const { return GetFixedFrameRate(); };

	/** Returns how many syncs occurred since the last tick */
	virtual uint32 GetLastSyncCountDelta() const PURE_VIRTUAL(UGenlockedCustomTimeStep::GetLastSyncCountDelta, return 1;);

	/** Returns true if the Sync related functions will return valid data */
	virtual bool IsLastSyncDataValid() const PURE_VIRTUAL(UGenlockedCustomTimeStep::IsLastSyncDataValid, return false;);
};
