// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMStructNode.h"
#include "RigVMModel/Nodes/RigVMVariableNode.h"
#include "RigVMModel/Nodes/RigVMParameterNode.h"
#include "RigVMModel/Nodes/RigVMBeginExecuteNode.h"
#include "RigVMModel/Nodes/RigVMCommentNode.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"
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

#if WITH_EDITOR
	URigVMStructNode* AddStructNode(UScriptStruct* InScriptStruct, const FName& InMethodName, const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);
	URigVMStructNode* AddStructNode(const FString& InScriptStructPath, const FName& InMethodName, const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);
	URigVMVariableNode* AddVariableNode(const FName& InVariableName, const FString& InCPPType, UScriptStruct* InScriptStruct, bool bIsGetter, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);
	URigVMParameterNode* AddParameterNode(const FName& InParameterName, const FString& InCPPType, UScriptStruct* InScriptStruct, bool bIsInput, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);
	URigVMBeginExecuteNode* AddBeginExecuteNode(const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);
	URigVMCommentNode* AddCommentNode(const FString& InCommentText, const FVector2D& InPosition = FVector2D::ZeroVector, const FVector2D& InSize = FVector2D(400.f, 300.f), bool bUndo = true);
	URigVMRerouteNode* AddRerouteNodeOnLink(URigVMLink* InLink, bool bShowAsFullNode, const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);
	URigVMRerouteNode* AddRerouteNodeOnLink(const FString& InLinkPinPathRepresentation, bool bShowAsFullNode, const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);
	URigVMRerouteNode* AddRerouteNodeOnPin(const FString& InPinPath, bool bShowAsFullNode, bool bAsInput, const FVector2D& InPosition = FVector2D::ZeroVector, bool bUndo = true);
#endif

	bool RemoveNode(URigVMNode* InNode, bool bUndo = true);
	bool RemoveNode(const FName& InNodeName, bool bUndo = true);

	bool SelectNode(URigVMNode* InNode, bool bSelect = true, bool bUndo = true);
	bool SelectNode(const FName& InNodeName, bool bSelect = true, bool bUndo = true);
	bool ClearNodeSelection(bool bUndo = true);

	bool SetNodePosition(URigVMNode* InNode, const FVector2D& InPosition, bool bUndo = true);
	bool SetNodePosition(const FName& InNodeName, const FVector2D& InPosition, bool bUndo = true);
	bool SetNodeSize(URigVMNode* InNode, const FVector2D& InSize, bool bUndo = true);
	bool SetNodeSize(const FName& InNodeName, const FVector2D& InSize, bool bUndo = true);
	bool SetCommentText(URigVMNode* InNode, const FString& InCommentText, bool bUndo = true);
	bool SetCommentText(const FName& InNodeName, const FString& InCommentText, bool bUndo = true);
	bool SetRerouteCompactness(URigVMNode* InNode, bool bShowAsFullNode, bool bUndo = true);
	bool SetRerouteCompactness(const FName& InNodeName, bool bShowAsFullNode, bool bUndo = true);
	bool RenameVariable(const FName& InOldName, const FName& InNewName, bool bUndo = true);
	bool RenameParameter(const FName& InOldName, const FName& InNewName, bool bUndo = true);

	FString GetPinDefaultValue(const FString& InPinPath);
	void SetPinDefaultValue(const FString& InPinPath, const FString& InDefaultValue, bool bUndo = true);

	FString AddArrayPin(const FString& InArrayPinPath, const FString& InDefaultValue = FString(), bool bUndo = true);
	FString DuplicateArrayPin(const FString& InArrayElementPinPath, bool bUndo = true);
	FString InsertArrayPin(const FString& InArrayPinPath, int32 InIndex = INDEX_NONE, const FString& InDefaultValue = FString(), bool bUndo = true);
	bool RemoveArrayPin(const FString& InArrayElementPinPath, bool bUndo = true);
	bool ClearArrayPin(const FString& InArrayPinPath, bool bUndo = true);

	bool AddLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bUndo = true);
	bool BreakLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bUndo = true);
	bool BreakAllLinks(const FString& InPinPath, bool bAsInput = true, bool bUndo = true);

private:

	FRigVMGraphModifiedEvent ModifiedEvent;
	void Notify(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	FString GetValidName(const FString& InPrefix);
	bool IsValidNodeForGraph(URigVMNode* InNode);
	bool IsValidLinkForGraph(URigVMLink* InLink);
	void AddPinsForStruct(UStruct* InStruct, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const FString& InDefaultValue);
	void AddPinsForArray(UArrayProperty* InArrayProperty, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const TArray<FString>& InDefaultValues);
	void ConfigurePinFromProperty(UProperty* InProperty, URigVMPin* InOutPin, ERigVMPinDirection InPinDirection = ERigVMPinDirection::Invalid);
	void ConfigurePinFromPin(URigVMPin* InOutPin, URigVMPin* InPin);
	virtual bool ShouldStructBeUnfolded(const UStruct* InStruct);
	void SetPinDefaultValue(URigVMPin* InPin, const FString& InDefaultValue, bool bResizeArrays, bool bUndo);
	static TArray<FString> SplitDefaultValue(const FString& InDefaultValue);
	URigVMPin* InsertArrayPin(URigVMPin* ArrayPin, int32 InIndex, const FString& InDefaultValue, bool bUndo);
	bool RemovePin(URigVMPin* InPinToRemove, bool bUndo);
	UProperty* FindPropertyForPin(const FString& InPinPath);
	bool AddLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bUndo);
	bool BreakLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bUndo);
	bool BreakAllLinks(URigVMPin* Pin, bool bAsInput, bool bUndo);
	void BreakAllLinksRecursive(URigVMPin* Pin, bool bAsInput, bool bTowardsParent, bool bUndo);

	void DetachLinksFromPinObjects();
	void ReattachLinksToPinObjects();

#if WITH_EDITOR
	void RepopulatePinsOnNode(URigVMNode* InNode);
#endif

	static FLinearColor GetColorFromMetadata(const FString& InMetadata);

	UPROPERTY()
	URigVMGraph* Graph;

	static int32 sRemovedPinIndex;

	friend class URigVMGraph;
};

