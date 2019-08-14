// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMParameterNode.generated.h"

USTRUCT(BlueprintType)
struct FRigVMGraphParameterDescription
{
	GENERATED_BODY()

public:

	bool operator ==(const FRigVMGraphParameterDescription& Other) const
	{
		return Name == Other.Name;
	}

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphParameterDescription)
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphParameterDescription)
	bool bIsInput;

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphParameterDescription)
	FString CPPType;

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphParameterDescription)
	UScriptStruct* ScriptStruct;

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphParameterDescription)
	FString DefaultValue;
};

UCLASS(BlueprintType)
class RIGVM_API URigVMParameterNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMParameterNode();

	virtual FString GetNodeTitle() const;

	UFUNCTION(BlueprintCallable, Category = RigVMParameterNode)
	FName GetParameterName() const;

	UFUNCTION(BlueprintCallable, Category = RigVMParameterNode)
	bool IsInput() const;

	UFUNCTION(BlueprintCallable, Category = RigVMParameterNode)
	FString GetCPPType() const;

	UFUNCTION(BlueprintCallable, Category = RigVMParameterNode)
	UScriptStruct* GetScriptStruct() const;

	UFUNCTION(BlueprintCallable, Category = RigVMParameterNode)
	FString GetDefaultValue() const;

	UFUNCTION(BlueprintCallable, Category = RigVMParameterNode)
	FRigVMGraphParameterDescription GetParameterDescription() const;

	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Green; }

	virtual bool ContributesToResult() const override { return !IsInput(); }

private:

	static const FString ValueName;

	UPROPERTY()
	FName ParameterName;

	friend class URigVMController;
};

