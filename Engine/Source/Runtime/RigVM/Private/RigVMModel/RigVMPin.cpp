// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/RigVMNode.h"

bool URigVMPin::SplitPinPathAtStart(const FString& InPinPath, FString& LeftMost, FString& Right)
{
	return InPinPath.Split(TEXT("."), &LeftMost, &Right, ESearchCase::IgnoreCase, ESearchDir::FromStart);
}

bool URigVMPin::SplitPinPathAtEnd(const FString& InPinPath, FString& Left, FString& RightMost)
{
	return InPinPath.Split(TEXT("."), &Left, &RightMost, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
}

bool URigVMPin::SplitPinPath(const FString& InPinPath, TArray<FString>& Parts)
{
	int32 OriginalPartsCount = Parts.Num();
	FString PinPathRemaining = InPinPath;
	FString Left, Right;
	while(SplitPinPathAtStart(PinPathRemaining, Left, Right))
	{
		Parts.Add(Left);
		Left.Empty();
		PinPathRemaining = Right;
	}

	if (!Right.IsEmpty())
	{
		Parts.Add(Right);
	}

	return Parts.Num() > OriginalPartsCount;
}

FString URigVMPin::JoinSplitPath(const FString& Left, const FString& Right)
{
	ensure(!Left.IsEmpty() && !Right.IsEmpty());
	return Left + TEXT(".") + Right;
}

FString URigVMPin::JoinSplitPath(const TArray<FString>& InParts)
{
	if (InParts.Num() == 0)
	{
		return FString();
	}

	FString Result = InParts[0];
	for (int32 PartIndex = 1; PartIndex < InParts.Num(); PartIndex++)
	{
		Result += TEXT(".") + InParts[PartIndex];
	}

	return Result;
}

URigVMPin::URigVMPin()
	: Direction(ERigVMPinDirection::Invalid)
	, bIsConstant(false)
	, Index(INDEX_NONE)
	, CPPType(FString())
{
}

FString URigVMPin::GetPinPath() const
{
	URigVMPin* ParentPin = GetParentPin();
	if(ParentPin)
	{
		return FString::Printf(TEXT("%s.%s"), *ParentPin->GetPinPath(), *GetName());
	}

	URigVMNode* Node = GetNode();
	return FString::Printf(TEXT("%s.%s"), *Node->GetNodePath(), *GetName());
}

ERigVMPinDirection URigVMPin::GetDirection() const
{
	return Direction;
}

bool URigVMPin::IsConstant() const
{
	return bIsConstant;
}

bool URigVMPin::IsArray() const
{
	return CPPType.StartsWith(TEXT("TArray"));
}

bool URigVMPin::IsArrayElement() const
{
	URigVMPin* ParentPin = GetParentPin();
	if (ParentPin == nullptr)
	{
		return false;
	}
	return ParentPin->IsArray();
}

int32 URigVMPin::GetIndex() const
{
	return Index;
}

FString URigVMPin::GetCPPType() const
{
	return CPPType;
}

URigVMPin* URigVMPin::GetParentPin() const
{
	return Cast<URigVMPin>(GetOuter());
}

const TArray<URigVMPin*>& URigVMPin::GetSubPins() const
{
	return SubPins;
}

URigVMPin* URigVMPin::FindSubPin(const FString& InPinPath) const
{
	FString Left, Right;
	if (!URigVMPin::SplitPinPathAtStart(InPinPath, Left, Right))
	{
		Left = InPinPath;
	}

	for (URigVMPin* Pin : SubPins)
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

const TArray<URigVMPin*>& URigVMPin::GetConnectedPins() const
{
	return ConnectedPins;
}

URigVMNode* URigVMPin::GetNode() const
{
	URigVMNode* Node = Cast<URigVMNode>(GetOuter());
	if(Node)
	{
		return Node;
	}

	URigVMPin* ParentPin = GetParentPin();
	if(ParentPin)
	{
		return ParentPin->GetNode();
	}

	return nullptr;
}

URigVMGraph* URigVMPin::GetGraph() const
{
	URigVMNode* Node = GetNode();
	if(Node)
	{
		return Node->GetGraph();
	}

	return nullptr;
}
