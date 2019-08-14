// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMCore/RigVMExecuteContext.h"

const FString URigVMNode::ExecuteName = TEXT("Execute");
const FString URigVMNode::NodeColorName = TEXT("NodeColor");

URigVMNode::URigVMNode()
: UObject()
, Position(FVector2D::ZeroVector)
, Size(FVector2D::ZeroVector)
, NodeColor(FLinearColor::Black)
{

}

URigVMNode::~URigVMNode()
{
}

FString URigVMNode::GetNodePath() const
{
	return GetName();
}

int32 URigVMNode::GetNodeIndex() const
{
	int32 Index = INDEX_NONE;
	URigVMGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		Graph->GetNodes().Find((URigVMNode*)this, Index);
	}
	return Index;
}

const TArray<URigVMPin*>& URigVMNode::GetPins() const
{
	return Pins;
}

URigVMPin* URigVMNode::FindPin(const FString& InPinPath) const
{
	FString Left, Right;
	if (!URigVMPin::SplitPinPathAtStart(InPinPath, Left, Right))
	{
		Left = InPinPath;
	}

	for (URigVMPin* Pin : Pins)
	{
		if (Pin->GetName() == Left)
		{
			if (Right.IsEmpty())
			{
				return Pin;
			}
			return Pin->FindSubPin(Right);
		}
	}
	return nullptr;
}

URigVMGraph* URigVMNode::GetGraph() const
{
	return Cast<URigVMGraph>(GetOuter());
}

FString URigVMNode::GetNodeTitle() const
{
	if (!NodeTitle.IsEmpty())
	{
		return NodeTitle;
	}
	return GetName();
}

FVector2D URigVMNode::GetPosition() const
{
	return Position;
}

FVector2D URigVMNode::GetSize() const
{
	return Size;
}

FLinearColor URigVMNode::GetNodeColor() const
{
	return NodeColor;
}

bool URigVMNode::IsSelected() const
{
	URigVMGraph* Graph = GetGraph();
	if (Graph)
	{
		return Graph->IsNodeSelected(GetFName());
	}
	return false;
}

bool URigVMNode::IsMutable() const
{
	URigVMPin* ExecutePin = FindPin(ExecuteName);
	if (ExecutePin)
	{
		if (ExecutePin->GetScriptStruct() == FRigVMExecuteContext::StaticStruct())
		{
			return true;
		}
	}
	return false;
}


bool URigVMNode::IsConnectedTo(URigVMNode* InNode) const
{
	if (InNode == nullptr)
	{
		return false;
	}
	if (InNode == this)
	{
		return false;
	}
	if (GetGraph() != InNode->GetGraph())
	{
		return false;
	}
	for (URigVMPin* Pin : Pins)
	{
		if (IsConnectedToRecursive(Pin, InNode))
		{
			return true;
		}
	}
	return false;
}

bool URigVMNode::IsConnectedToRecursive(URigVMPin* InPin, URigVMNode* InNode) const
{
	for (URigVMPin* ConnectedPin : InPin->GetConnectedSourcePins())
	{
		if (ConnectedPin->GetNode() == InNode)
		{
			return true;
		}
	}
	for (URigVMPin* ConnectedPin : InPin->GetConnectedTargetPins())
	{
		if (ConnectedPin->GetNode() == InNode)
		{
			return true;
		}
	}
	for (URigVMPin* SubPin : InPin->GetSubPins())
	{
		if (IsConnectedToRecursive(SubPin, InNode))
		{
			return true;
		}
	}
	return false;
}

TArray<URigVMNode*> URigVMNode::GetConnectedSourceNodes() const
{
	TArray<URigVMNode*> Nodes;
	for (URigVMPin* Pin : Pins)
	{
		GetConnectedNodesRecursive(Pin, true, Nodes);
	}
	return Nodes;
}

TArray<URigVMNode*> URigVMNode::GetConnectedTargetNodes() const
{
	TArray<URigVMNode*> Nodes;
	for (URigVMPin* Pin : Pins)
	{
		GetConnectedNodesRecursive(Pin, false, Nodes);
	}
	return Nodes;
}

void URigVMNode::GetConnectedNodesRecursive(URigVMPin* InPin, bool bLookForSources, TArray<URigVMNode*>& OutNodes) const
{
	TArray<URigVMPin*> ConnectedPins = bLookForSources ? InPin->GetConnectedSourcePins() : InPin->GetConnectedTargetPins();
	for (URigVMPin* ConnectedPin : ConnectedPins)
	{
		OutNodes.AddUnique(ConnectedPin->GetNode());
	}
	for (URigVMPin* SubPin : InPin->GetSubPins())
	{
		GetConnectedNodesRecursive(SubPin, bLookForSources, OutNodes);
	}
}
