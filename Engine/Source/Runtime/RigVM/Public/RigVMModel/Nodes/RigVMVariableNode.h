// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMVariableNode.generated.h"

USTRUCT()
struct FRigVMGraphVariableDescription
{
	GENERATED_BODY()

public:

	bool operator ==(const FRigVMGraphVariableDescription& Other) const
	{
		return Name == Other.Name;
	}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	UScriptStruct* ScriptStruct;

	UPROPERTY()
	FString DefaultValue;
};

UCLASS()
class RIGVM_API URigVMVariableNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMVariableNode();

	virtual FString GetNodeTitle() const;
	FName GetVariableName() const;
	bool IsGetter() const;
	FString GetCPPType() const;
	UScriptStruct* GetScriptStruct() const;
	FString GetDefaultValue() const;
	FRigVMGraphVariableDescription GetVariableDescription() const;
	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Blue; }

private:

	static const FString ValueName;

	UPROPERTY()
	FName VariableName;

	friend class URigVMController;
};

