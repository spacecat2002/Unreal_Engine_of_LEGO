// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMNode.h"
#include "RigVMRerouteNode.generated.h"

UCLASS()
class RIGVM_API URigVMRerouteNode : public URigVMNode
{
	GENERATED_BODY()

public:

	URigVMRerouteNode();

	bool GetShowsAsFullNode() const;

private:

	UPROPERTY()
	bool bShowAsFullNode;

	friend class URigVMController;
};

