// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMBeginExecuteNode.generated.h"

UCLASS(BlueprintType)
class RIGVM_API URigVMBeginExecuteNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMBeginExecuteNode();

	virtual FString GetNodeTitle() const override;

private:

	static const FString BeginName;

	friend class URigVMController;
};

