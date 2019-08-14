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

	URigVMNode();
	virtual ~URigVMNode();

	FString GetNodePath() const;
	int32 GetNodeIndex() const;

	const TArray<URigVMPin*>& GetPins() const;
	URigVMPin* FindPin(const FString& InPinPath) const;

	URigVMGraph* GetGraph() const;

	virtual FString GetNodeTitle() const;
	FVector2D GetPosition() const;
	FVector2D GetSize() const;

	bool IsSelected() const;

	virtual bool IsPure() const { return !IsMutable(); }
	virtual bool IsMutable() const;
	virtual bool ContributesToResult() const { return IsMutable(); }

	bool IsConnectedTo(URigVMNode* InNode) const;

	TArray<URigVMNode*> GetConnectedSourceNodes() const;
	TArray<URigVMNode*> GetConnectedTargetNodes() const;

private:

	static const FString ExecuteName;

	bool IsConnectedToRecursive(URigVMPin* InPin, URigVMNode* InNode) const;
	void GetConnectedNodesRecursive(URigVMPin* InPin, bool bLookForSources, TArray<URigVMNode*>& OutNodes) const;

protected:

	UPROPERTY()
	FString NodeTitle;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FVector2D Size;

private:
	UPROPERTY()
	TArray<URigVMPin*> Pins;

	friend class URigVMController;
	friend class RigVMGraph;
};

