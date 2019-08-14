// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMBeginExecuteNode.h"

const FString URigVMBeginExecuteNode::BeginName = TEXT("Begin");

URigVMBeginExecuteNode::URigVMBeginExecuteNode()
{
}

FString URigVMBeginExecuteNode::GetNodeTitle() const
{
	return BeginName;
}
