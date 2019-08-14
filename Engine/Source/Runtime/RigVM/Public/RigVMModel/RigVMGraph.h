// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMNode.h"
#include "RigVMLink.h"
#include "RigVMNotifications.h"
#include "RigVMGraphUtils.h"
#include "RigVMVariableNode.h"
#include "RigVMGraph.generated.h"

UCLASS()
class RIGVM_API URigVMGraph : public UObject
{
	GENERATED_BODY()

public:

	URigVMGraph();

	const TArray<URigVMNode*>& GetNodes() const;
	const TArray<URigVMLink*>& GetLinks() const;
	TArray<FRigVMGraphVariableDescription> GetVariableDescriptions() const;
	URigVMNode* FindNode(const FName& InNodeName) const;
	URigVMNode* FindNode(const FString& InNodePath) const;
	URigVMPin* FindPin(const FString& InPinPath) const;

	bool IsNodeSelected(const FName& InNodeName) const;
	const TArray<FName>& GetSelectNodes() const;

	void PrepareCycleChecking(URigVMPin* InPin, bool bAsInput);

	FRigVMGraphModifiedEvent& OnModified();

#if WITH_EDITORONLY_DATA
	virtual void PostLoad() override;
#endif

protected:

	virtual bool CanLink(URigVMPin* InSourcePin, URigVMPin* InTargetPin, FString* OutFailureReason = nullptr);

private:

	void RepopulatePinLinks();

	FRigVMGraphModifiedEvent ModifiedEvent;
	void Notify(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	UPROPERTY()
	TArray<URigVMNode*> Nodes;

	UPROPERTY()
	TArray<URigVMLink*> Links;

	UPROPERTY()
	TArray<FName> SelectedNodes;

	FRigVMGraphUtils Utils;

	bool IsNameAvailable(const FString& InName);

	friend class URigVMController;
};

