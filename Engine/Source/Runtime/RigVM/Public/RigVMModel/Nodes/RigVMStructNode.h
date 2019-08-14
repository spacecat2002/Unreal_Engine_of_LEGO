// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMStructNode.generated.h"

UCLASS(BlueprintType)
class RIGVM_API URigVMStructNode : public URigVMNode
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = RigVMStructNode)
	UScriptStruct* GetScriptStruct() const;

	UFUNCTION(BlueprintCallable, Category = RigVMStructNode)
	FName GetMethodName() const;

private:

	UPROPERTY()
	UScriptStruct* ScriptStruct;

	UPROPERTY()
	FName MethodName;

	friend class URigVMController;
};

