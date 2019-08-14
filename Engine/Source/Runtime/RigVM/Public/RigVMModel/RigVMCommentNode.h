// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMNode.h"
#include "RigVMCommentNode.generated.h"

UCLASS()
class RIGVM_API URigVMCommentNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMCommentNode();

	virtual FString GetNodeTitle() const override { return GetCommentText(); }
	FString GetCommentText() const;

private:

	UPROPERTY()
	FString CommentText;

	friend class URigVMController;
};

