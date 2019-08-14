// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMPin.h"
#include "RigVMNode.generated.h"

class URigVMGraph;

UCLASS(BlueprintType)
class RIGVM_API URigVMNode : public UObject
{
	GENERATED_BODY()

public:

	URigVMNode();
	virtual ~URigVMNode();

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	FString GetNodePath() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	int32 GetNodeIndex() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	const TArray<URigVMPin*>& GetPins() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	URigVMPin* FindPin(const FString& InPinPath) const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	URigVMGraph* GetGraph() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	virtual FString GetNodeTitle() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	FVector2D GetPosition() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	FVector2D GetSize() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	virtual FLinearColor GetNodeColor() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	bool IsSelected() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	virtual bool IsPure() const { return !IsMutable(); }

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	virtual bool IsMutable() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	virtual bool ContributesToResult() const { return IsMutable(); }

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	bool IsConnectedTo(URigVMNode* InNode) const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	TArray<URigVMNode*> GetConnectedSourceNodes() const;

	UFUNCTION(BlueprintCallable, Category = RigVMNode)
	TArray<URigVMNode*> GetConnectedTargetNodes() const;

private:

	static const FString ExecuteName;
	static const FString NodeColorName;

	bool IsConnectedToRecursive(URigVMPin* InPin, URigVMNode* InNode) const;
	void GetConnectedNodesRecursive(URigVMPin* InPin, bool bLookForSources, TArray<URigVMNode*>& OutNodes) const;

protected:

	UPROPERTY()
	FString NodeTitle;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FVector2D Size;

	UPROPERTY()
	FLinearColor NodeColor;

private:
	UPROPERTY()
	TArray<URigVMPin*> Pins;

	friend class URigVMController;
	friend class RigVMGraph;
};

