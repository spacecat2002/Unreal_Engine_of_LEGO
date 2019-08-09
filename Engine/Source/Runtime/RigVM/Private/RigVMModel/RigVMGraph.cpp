// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMStructNode.h"

const TArray<URigVMNode*>& URigVMGraph::GetNodes() const
{
	return Nodes;
}

URigVMNode* URigVMGraph::FindNode(const FName& InNodeName) const
{
	for (URigVMNode* Node : Nodes)
	{
		if (Node->GetFName() == InNodeName)
		{
			return Node;
		}
	}
	return nullptr;
}

URigVMNode* URigVMGraph::FindNode(const FString& InNodePath) const
{
	// for now we don't support encapsulation
	// so we don't walk recursively
	return FindNode(FName(*InNodePath));
}

URigVMPin* URigVMGraph::FindPin(const FString& InPinPath) const
{
	FString Left, Right;
	if (!URigVMPin::SplitPinPathAtStart(InPinPath, Left, Right))
	{
		Left = InPinPath;
	}

	URigVMNode* Node = FindNode(Left);
	if (Node)
	{
		return Node->FindPin(Right);
	}

	return nullptr;
}

bool URigVMGraph::IsNodeSelected(const FName& InNodeName) const
{
	return SelectedNodes.Contains(InNodeName);
}

const TArray<FName>& URigVMGraph::GetSelectNodes() const
{
	return SelectedNodes;
}

FRigVMGraphModifiedEvent& URigVMGraph::OnModified()
{
	return ModifiedEvent;
}

void URigVMGraph::Notify(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	ModifiedEvent.Broadcast(InNotifType, InGraph, InSubject);
}

bool URigVMGraph::IsNameAvailable(const FString& InName)
{
	for (URigVMNode* Node : Nodes)
	{
		if (Node->GetName() == InName)
		{
			return false;
		}
	}
	return true;
}
