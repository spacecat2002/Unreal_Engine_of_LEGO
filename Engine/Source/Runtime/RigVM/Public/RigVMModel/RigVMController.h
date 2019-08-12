// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMGraph.h"
#include "RigVMModel/RigVMStructNode.h"
#include "RigVMController.generated.h"

UCLASS()
class RIGVM_API URigVMController : public UObject
{
	GENERATED_BODY()

public:

	~URigVMController();

	URigVMGraph* GetGraph() const;
	void SetGraph(URigVMGraph* InGraph);

	FRigVMGraphModifiedEvent& OnModified();

	URigVMStructNode* AddStructNode(UScriptStruct* InScriptStruct, const FName& InMethodName, const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);

	bool RemoveNode(URigVMNode* InNode, bool bUndo = true);
	bool RemoveNode(const FName& InNodeName, bool bUndo = true);

	bool SelectNode(URigVMNode* InNode, bool bSelect = true, bool bUndo = true);
	bool SelectNode(const FName& InNodeName, bool bSelect = true, bool bUndo = true);
	bool ClearNodeSelection(bool bUndo = true);

	FString GetPinDefaultValue(const FString& InPinPath);
	void SetPinDefaultValue(const FString& InPinPath, const FString& InDefaultValue);

	FString AddArrayPin(const FString& InArrayPinPath, const FString& InDefaultValue = FString());
	FString DuplicateArrayPin(const FString& InArrayElementPinPath);
	FString InsertArrayPin(const FString& InArrayPinPath, int32 InIndex = INDEX_NONE, const FString& InDefaultValue = FString());
	bool RemoveArrayPin(const FString& InArrayElementPinPath);
	bool ClearArrayPin(const FString& InArrayPinPath);

	bool AddLink(const FString& InOutputPinPath, const FString& InInputPinPath);
	bool BreakLink(const FString& InOutputPinPath, const FString& InInputPinPath);
	bool BreakAllLinks(const FString& InPinPath, bool bAsInput = true);

private:

	FRigVMGraphModifiedEvent ModifiedEvent;
	void Notify(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	bool IsValidNodeForGraph(URigVMNode* InNode);
	void AddPinsForStruct(UStruct* InStruct, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const FString& InDefaultValue);
	void AddPinsForArray(UArrayProperty* InArrayProperty, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const TArray<FString>& InDefaultValues);
	void ConfigurePinFromProperty(UProperty* InProperty, URigVMPin* InOutPin, ERigVMPinDirection InPinDirection = ERigVMPinDirection::Invalid);
	void ConfigurePinFromPin(URigVMPin* InOutPin, URigVMPin* InPin);
	virtual bool ShouldStructBeUnfolded(const UStruct* InStruct);
	void SetPinDefaultValue(URigVMPin* InPin, const FString& InDefaultValue);
	static TArray<FString> SplitDefaultValue(const FString& InDefaultValue);
	URigVMPin* InsertArrayPin(URigVMPin* ArrayPin, int32 InIndex, const FString& InDefaultValue);
	bool RemovePin(URigVMPin* InPinToRemove);
	UProperty* FindPropertyForPin(const FString& InPinPath);
	bool AddLink(URigVMPin* OutputPin, URigVMPin* InputPin);
	bool BreakLink(URigVMPin* OutputPin, URigVMPin* InputPin);
	bool BreakAllLinks(URigVMPin* Pin, bool bAsInput = true);
	void BreakAllLinksRecursive(URigVMPin* Pin, bool bAsInput, bool bTowardsParent);

	UPROPERTY()
	URigVMGraph* Graph;

	static int32 sRemovedPinIndex;
};

