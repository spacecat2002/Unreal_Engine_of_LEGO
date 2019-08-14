// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMParameterNode.generated.h"

USTRUCT()
struct FRigVMGraphParameterDescription
{
	GENERATED_BODY()

public:

	bool operator ==(const FRigVMGraphParameterDescription& Other) const
	{
		return Name == Other.Name;
	}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	bool bIsInput;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	UScriptStruct* ScriptStruct;

	UPROPERTY()
	FString DefaultValue;
};

UCLASS()
class RIGVM_API URigVMParameterNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMParameterNode();

	virtual FString GetNodeTitle() const;
	FName GetParameterName() const;
	bool IsInput() const;
	FString GetCPPType() const;
	UScriptStruct* GetScriptStruct() const;
	FString GetDefaultValue() const;
	FRigVMGraphParameterDescription GetParameterDescription() const;

	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Green; }
	virtual bool ContributesToResult() const override { return !IsInput(); }

private:

	static const FString ValueName;

	UPROPERTY()
	FName ParameterName;

	friend class URigVMController;
};

