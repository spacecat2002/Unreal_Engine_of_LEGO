// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMGraph.h"

FString URigVMNode::GetNodePath() const
{
	return GetName();
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

FVector2D URigVMNode::GetPosition() const
{
	return Position;
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
