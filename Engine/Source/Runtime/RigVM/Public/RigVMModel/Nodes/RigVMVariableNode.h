// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMVariableNode.generated.h"

USTRUCT(BlueprintType)
struct FRigVMGraphVariableDescription
{
	GENERATED_BODY()

public:

	bool operator ==(const FRigVMGraphVariableDescription& Other) const
	{
		return Name == Other.Name;
	}

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphVariableDescription)
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphVariableDescription)
	FString CPPType;

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphVariableDescription)
	UScriptStruct* ScriptStruct;

	UPROPERTY(BlueprintReadOnly, Category = RigVMGraphVariableDescription)
	FString DefaultValue;
};

UCLASS(BlueprintType)
class RIGVM_API URigVMVariableNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMVariableNode();

	virtual FString GetNodeTitle() const;

	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	FName GetVariableName() const;

	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	bool IsGetter() const;

	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	FString GetCPPType() const;

	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	UScriptStruct* GetScriptStruct() const;

	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	FString GetDefaultValue() const;

	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	FRigVMGraphVariableDescription GetVariableDescription() const;

	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Blue; }

private:

	static const FString ValueName;

	UPROPERTY()
	FName VariableName;

	friend class URigVMController;
};

