// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMNode.h"
#include "RigVMLink.h"
#include "RigVMNotifications.h"
#include "RigVMGraphUtils.h"
#include "Nodes/RigVMVariableNode.h"
#include "Nodes/RigVMParameterNode.h"
#include "RigVMGraph.generated.h"

UCLASS(BlueprintType)
class RIGVM_API URigVMGraph : public UObject
{
	GENERATED_BODY()

public:

	URigVMGraph();

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	const TArray<URigVMNode*>& GetNodes() const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	const TArray<URigVMLink*>& GetLinks() const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	TArray<FRigVMGraphVariableDescription> GetVariableDescriptions() const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	TArray<FRigVMGraphParameterDescription> GetParameterDescriptions() const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMNode* FindNodeByName(const FName& InNodeName) const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMNode* FindNode(const FString& InNodePath) const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMPin* FindPin(const FString& InPinPath) const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMLink* FindLink(const FString& InLinkPinPathRepresentation) const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	bool IsNodeSelected(const FName& InNodeName) const;

	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
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

