// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMRerouteNode.generated.h"

UCLASS(BlueprintType)
class RIGVM_API URigVMRerouteNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMRerouteNode();

	virtual FString GetNodeTitle() const override { return RerouteName; }

	UFUNCTION(BlueprintCallable, Category = RigVMRerouteNode)
	bool GetShowsAsFullNode() const;

private:

	static const FString RerouteName;
	static const FString ValueName;

	UPROPERTY()
	bool bShowAsFullNode;

	friend class URigVMController;
};

