// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMStructNode.h"
#include "RigVMModel/Nodes/RigVMVariableNode.h"
#include "RigVMModel/Nodes/RigVMParameterNode.h"
#include "RigVMModel/Nodes/RigVMCommentNode.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"
#include "RigVMController.generated.h"

class URigVMActionStack;

UCLASS(BlueprintType)
class RIGVM_API URigVMController : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	URigVMController();
	~URigVMController();

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMGraph* GetGraph() const;

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void SetGraph(URigVMGraph* InGraph);

	FRigVMGraphModifiedEvent& OnModified();

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void EnableReporting(bool bEnabled = true) { bReportWarningsAndErrors = bEnabled; }

#if WITH_EDITOR

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMStructNode* AddStructNode(UScriptStruct* InScriptStruct, const FName& InMethodName, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMStructNode* AddStructNodeFromStructPath(const FString& InScriptStructPath, const FName& InMethodName, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMVariableNode* AddVariableNode(const FName& InVariableName, const FString& InCPPType, UScriptStruct* InScriptStruct, bool bIsGetter, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMVariableNode* AddVariableNodeFromStructPath(const FName& InVariableName, const FString& InCPPType, const FString& InScriptStructPath, bool bIsGetter, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMParameterNode* AddParameterNode(const FName& InParameterName, const FString& InCPPType, UScriptStruct* InScriptStruct, bool bIsInput, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMParameterNode* AddParameterNodeFromStructPath(const FName& InParameterName, const FString& InCPPType, const FString& InScriptStructPath, bool bIsInput, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMCommentNode* AddCommentNode(const FString& InCommentText, const FVector2D& InPosition = FVector2D::ZeroVector, const FVector2D& InSize = FVector2D(400.f, 300.f), const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddRerouteNodeOnLink(URigVMLink* InLink, bool bShowAsFullNode, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddRerouteNodeOnLinkPath(const FString& InLinkPinPathRepresentation, bool bShowAsFullNode, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddRerouteNodeOnPin(const FString& InPinPath, bool bShowAsFullNode, bool bAsInput, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool Undo();

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool Redo();

#endif

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveNode(URigVMNode* InNode, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveNodeByName(const FName& InNodeName, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SelectNode(URigVMNode* InNode, bool bSelect = true, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SelectNodeByName(const FName& InNodeName, bool bSelect = true, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool ClearNodeSelection(bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodePosition(URigVMNode* InNode, const FVector2D& InPosition, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodePositionByName(const FName& InNodeName, const FVector2D& InPosition, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeSize(URigVMNode* InNode, const FVector2D& InSize, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeSizeByName(const FName& InNodeName, const FVector2D& InSize, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetCommentText(URigVMNode* InNode, const FString& InCommentText, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetCommentTextByName(const FName& InNodeName, const FString& InCommentText, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetRerouteCompactness(URigVMNode* InNode, bool bShowAsFullNode, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetRerouteCompactnessByName(const FName& InNodeName, bool bShowAsFullNode, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RenameVariable(const FName& InOldName, const FName& InNewName, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RenameParameter(const FName& InOldName, const FName& InNewName, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString GetPinDefaultValue(const FString& InPinPath);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void SetPinDefaultValue(const FString& InPinPath, const FString& InDefaultValue, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString AddArrayPin(const FString& InArrayPinPath, const FString& InDefaultValue = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString DuplicateArrayPin(const FString& InArrayElementPinPath, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString InsertArrayPin(const FString& InArrayPinPath, int32 InIndex = -1, const FString& InDefaultValue = TEXT(""), bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveArrayPin(const FString& InArrayElementPinPath, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool ClearArrayPin(const FString& InArrayPinPath, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool AddLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool BreakLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bUndo = true);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool BreakAllLinks(const FString& InPinPath, bool bAsInput = true, bool bUndo = true);

private:

	UPROPERTY(BlueprintReadOnly, Category = RigVMController, meta = (ScriptName = "ModifiedEvent", AllowPrivateAccess = "true"))
	FRigVMGraphModifiedDynamicEvent ModifiedEventDynamic;

	FRigVMGraphModifiedEvent ModifiedEventStatic;
	void Notify(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	FString GetValidName(const FString& InPrefix);
	bool IsValidGraph();
	bool IsValidNodeForGraph(URigVMNode* InNode);
	bool IsValidPinForGraph(URigVMPin* InPin);
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

	void ReportWarning(const FString& InMessage);
	void ReportError(const FString& InMessage);

	template <typename FmtType, typename... Types>
	void ReportWarningf(const FmtType& Fmt, Types... Args)
	{
		ReportWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportErrorf(const FmtType& Fmt, Types... Args)
	{
		ReportError(FString::Printf(Fmt, Args...));
	}

	void DetachLinksFromPinObjects();
	void ReattachLinksToPinObjects();

#if WITH_EDITOR
	void RepopulatePinsOnNode(URigVMNode* InNode);
#endif

	static FLinearColor GetColorFromMetadata(const FString& InMetadata);

	UPROPERTY()
	URigVMGraph* Graph;

	UPROPERTY(transient)
	URigVMActionStack* ActionStack;

	bool bReportWarningsAndErrors;
	static int32 sRemovedPinIndex;

	friend class URigVMGraph;
	friend class URigVMActionStack;
};

