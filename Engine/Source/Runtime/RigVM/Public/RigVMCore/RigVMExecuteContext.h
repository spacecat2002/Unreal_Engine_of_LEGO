// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMArray.h"
#include "RigVMExecuteContext.generated.h"

/**
 * The execute context is used for mutable nodes to
 * indicate execution order.
 */
USTRUCT()
struct RIGVM_API FRigVMExecuteContext
{
	GENERATED_BODY()

	FRigVMExecuteContext()
	: EventName(NAME_None)
	{
	}

	FName EventName;

	FRigVMFixedArray<void*> OpaqueArguments;
};
