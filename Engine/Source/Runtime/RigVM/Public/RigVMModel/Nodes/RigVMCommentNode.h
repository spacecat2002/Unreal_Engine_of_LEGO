// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMCommentNode.generated.h"

UCLASS(BlueprintType)
class RIGVM_API URigVMCommentNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMCommentNode();

	virtual FString GetNodeTitle() const override { return GetCommentText(); }

	UFUNCTION(BlueprintCallable, Category = RigVMCommentNode)
	FString GetCommentText() const;

private:

	UPROPERTY()
	FString CommentText;

	friend class URigVMController;
};

