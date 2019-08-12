// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMPin.h"
#include "RigVMNode.generated.h"

class URigVMGraph;

UCLASS()
class RIGVM_API URigVMNode : public UObject
{
	GENERATED_BODY()

public:

	FString GetNodePath() const;

	const TArray<URigVMPin*>& GetPins() const;

	URigVMPin* FindPin(const FString& InPinPath) const;

	URigVMGraph* GetGraph() const;

	FVector2D GetPosition() const;

	bool IsSelected() const;

	bool IsConnectedTo(URigVMNode* InNode) const;
	TArray<URigVMNode*> GetConnectedSourceNodes() const;
	TArray<URigVMNode*> GetConnectedTargetNodes() const;

private:

	bool IsConnectedToRecursive(URigVMPin* InPin, URigVMNode* InNode) const;
	void GetConnectedNodesRecursive(URigVMPin* InPin, bool bLookForSources, TArray<URigVMNode*> OutNodes) const;

	UPROPERTY()
	TArray<URigVMPin*> Pins;

	UPROPERTY()
	FVector2D Position;

	friend class URigVMController;
};

