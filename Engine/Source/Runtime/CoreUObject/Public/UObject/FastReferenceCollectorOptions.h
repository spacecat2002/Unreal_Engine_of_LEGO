// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/*===================================================================================
	FastReferenceCollectorOptions.h: Global TFastReferenceCollector enums and defines
=====================================================================================*/

enum class EFastReferenceCollectorOptions : int32
{
	None = 0,
	Parallel = 1 << 0,
	AutogenerateTokenStream = 1 << 1,
	IgnoreNoOpTokens = 1 << 2,
	WithClusters = 1 << 3, 
};
ENUM_CLASS_FLAGS(EFastReferenceCollectorOptions);
