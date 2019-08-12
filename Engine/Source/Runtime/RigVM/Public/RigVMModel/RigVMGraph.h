// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMNode.h"
#include "RigVMLink.h"
#include "RigVMNotifications.h"
#include "RigVMGraph.generated.h"

UCLASS()
class RIGVM_API URigVMGraph : public UObject
{
	GENERATED_BODY()

public:

	const TArray<URigVMNode*>& GetNodes() const;
	const TArray<URigVMLink*>& GetLinks() const;
	URigVMNode* FindNode(const FName& InNodeName) const;
	URigVMNode* FindNode(const FString& InNodePath) const;
	URigVMPin* FindPin(const FString& InPinPath) const;

	bool IsNodeSelected(const FName& InNodeName) const;
	const TArray<FName>& GetSelectNodes() const;

	FRigVMGraphModifiedEvent& OnModified();

private:

	FRigVMGraphModifiedEvent ModifiedEvent;
	void Notify(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	UPROPERTY()
	TArray<URigVMNode*> Nodes;

	UPROPERTY()
	TArray<URigVMLink*> Links;

	UPROPERTY()
	TArray<FName> SelectedNodes;

	bool IsNameAvailable(const FString& InName);

	friend class URigVMController;
};

