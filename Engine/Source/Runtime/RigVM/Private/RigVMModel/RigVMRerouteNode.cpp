// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMRerouteNode.h"

URigVMRerouteNode::URigVMRerouteNode()
: bShowAsFullNode(true)
{
}

bool URigVMRerouteNode::GetShowsAsFullNode() const
{
	return bShowAsFullNode;
}