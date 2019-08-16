// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMControllerActions.h"
#include "UObject/Package.h"

FRigVMActionWrapper::FRigVMActionWrapper(const FRigVMActionKey& Key)
{
	ScriptStruct = FindObjectChecked<UScriptStruct>(ANY_PACKAGE, *Key.ScriptStructPath);
	Data.SetNumUninitialized(ScriptStruct->GetStructureSize());
	ScriptStruct->InitializeStruct(Data.GetData(), 1);
	ScriptStruct->ImportText(*Key.ExportedText, Data.GetData(), nullptr, PPF_None, nullptr, ScriptStruct->GetName());
}
FRigVMActionWrapper::~FRigVMActionWrapper()
{
	if (Data.Num() > 0 && ScriptStruct != nullptr)
	{
		ScriptStruct->DestroyStruct(Data.GetData(), 1);
	}
}

FRigVMBaseAction* FRigVMActionWrapper::GetAction()
{
	return (FRigVMBaseAction*)Data.GetData();
}

bool URigVMActionStack::Undo(URigVMController* InController)
{
	check(InController)

	if (UndoActions.Num() == 0)
	{
		InController->ReportWarning(TEXT("Nothing to undo."));
		return false;
	}

	FRigVMActionKey KeyToUndo = UndoActions.Pop();
	FRigVMActionWrapper Wrapper(KeyToUndo);
	if (Wrapper.GetAction()->Undo(InController))
	{
		RedoActions.Add(KeyToUndo);
		return true;
	}
	return false;
}

bool URigVMActionStack::Redo(URigVMController* InController)
{
	check(InController)

	if (RedoActions.Num() == 0)
	{
		InController->ReportWarning(TEXT("Nothing to redo."));
		return false;
	}

	FRigVMActionKey KeyToRedo = RedoActions.Pop();
	FRigVMActionWrapper Wrapper(KeyToRedo);
	if (Wrapper.GetAction()->Redo(InController))
	{
		UndoActions.Add(KeyToRedo);
		return true;
	}
	return false;
}

bool FRigVMBaseAction::Undo(URigVMController* InController)
{
	bool Result = true;
	for (int32 KeyIndex = SubActions.Num() - 1; KeyIndex >= 0; KeyIndex--)
	{
		FRigVMActionWrapper Wrapper(SubActions[KeyIndex]);
		if(!Wrapper.GetAction()->Undo(InController))
		{
			Result = false;
		}
	}
	return Result;
}

bool FRigVMBaseAction::Redo(URigVMController* InController)
{
	bool Result = true;
	for (int32 KeyIndex = 0; KeyIndex < SubActions.Num(); KeyIndex++)
	{
		FRigVMActionWrapper Wrapper(SubActions[KeyIndex]);
		if (!Wrapper.GetAction()->Redo(InController))
		{
			Result = false;
		}
	}
	return Result;
}

bool FRigVMInverseAction::Undo(URigVMController* InController)
{
	return FRigVMBaseAction::Redo(InController);
}

bool FRigVMInverseAction::Redo(URigVMController* InController)
{
	return FRigVMBaseAction::Undo(InController);
}

FRigVMAddStructNodeAction::FRigVMAddStructNodeAction()
	: ScriptStructPath()
	, MethodName(NAME_None)
	, Position(FVector2D::ZeroVector)
	, NodePath()
{
}

FRigVMAddStructNodeAction::FRigVMAddStructNodeAction(URigVMStructNode* InNode)
	: ScriptStructPath(InNode->GetScriptStruct()->GetPathName())
	, MethodName(InNode->GetMethodName())
	, Position(InNode->GetPosition())
	, NodePath(InNode->GetNodePath())
{
}

bool FRigVMAddStructNodeAction::Undo(URigVMController* InController)
{
	if (!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->RemoveNodeByName(*NodePath, false);
}

bool FRigVMAddStructNodeAction::Redo(URigVMController* InController)
{
#if WITH_EDITOR
	if (URigVMStructNode* Node = InController->AddStructNodeFromStructPath(ScriptStructPath, MethodName, Position, NodePath, false))
	{
		return FRigVMBaseAction::Redo(InController);
	}
#endif
	return false;
}

FRigVMAddVariableNodeAction::FRigVMAddVariableNodeAction()
	: VariableName(NAME_None)
	, CPPType()
	, ScriptStructPath()
	, bIsGetter(false)
	, DefaultValue()
	, Position(FVector2D::ZeroVector)
	, NodePath()
{
}

FRigVMAddVariableNodeAction::FRigVMAddVariableNodeAction(URigVMVariableNode* InNode)
	: VariableName(InNode->GetVariableName())
	, CPPType(InNode->GetCPPType())
	, ScriptStructPath(InNode->GetScriptStruct()->GetPathName())
	, bIsGetter(InNode->IsGetter())
	, DefaultValue(InNode->GetDefaultValue())
	, Position(InNode->GetPosition())
	, NodePath(InNode->GetNodePath())
{
}

bool FRigVMAddVariableNodeAction::Undo(URigVMController* InController)
{
	if (!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->RemoveNodeByName(*NodePath, false);
}

bool FRigVMAddVariableNodeAction::Redo(URigVMController* InController)
{
#if WITH_EDITOR
	if (URigVMVariableNode* Node = InController->AddVariableNodeFromStructPath(VariableName, CPPType, ScriptStructPath, bIsGetter, DefaultValue, Position, NodePath, false))
	{
		return FRigVMBaseAction::Redo(InController);
	}
#endif
	return false;
}

FRigVMAddParameterNodeAction::FRigVMAddParameterNodeAction()
	: ParameterName(NAME_None)
	, CPPType()
	, ScriptStructPath()
	, bIsInput(false)
	, DefaultValue()
	, Position(FVector2D::ZeroVector)
	, NodePath()
{
}

FRigVMAddParameterNodeAction::FRigVMAddParameterNodeAction(URigVMParameterNode* InNode)
	: ParameterName(InNode->GetParameterName())
	, CPPType(InNode->GetCPPType())
	, ScriptStructPath(InNode->GetScriptStruct()->GetPathName())
	, bIsInput(InNode->IsInput())
	, DefaultValue(InNode->GetDefaultValue())
	, Position(InNode->GetPosition())
	, NodePath(InNode->GetNodePath())
{
}

bool FRigVMAddParameterNodeAction::Undo(URigVMController* InController)
{
	if (!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->RemoveNodeByName(*NodePath, false);
}

bool FRigVMAddParameterNodeAction::Redo(URigVMController* InController)
{
#if WITH_EDITOR
	if (URigVMParameterNode* Node = InController->AddParameterNodeFromStructPath(ParameterName, CPPType, ScriptStructPath, bIsInput, DefaultValue, Position, NodePath, false))
	{
		return FRigVMBaseAction::Redo(InController);
	}
#endif
	return false;
}

FRigVMAddCommentNodeAction::FRigVMAddCommentNodeAction()
	: CommentText()
	, Position(FVector2D::ZeroVector)
	, Size(FVector2D::ZeroVector)
	, NodePath()
{
}

FRigVMAddCommentNodeAction::FRigVMAddCommentNodeAction(URigVMCommentNode* InNode)
	: CommentText(InNode->GetCommentText())
	, Position(InNode->GetPosition())
	, Size(InNode->GetSize())
	, NodePath(InNode->GetNodePath())
{
}

bool FRigVMAddCommentNodeAction::Undo(URigVMController* InController)
{
	if (!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->RemoveNodeByName(*NodePath, false);
}

bool FRigVMAddCommentNodeAction::Redo(URigVMController* InController)
{
#if WITH_EDITOR
	if (URigVMCommentNode* Node = InController->AddCommentNode(CommentText, Position, Size, NodePath, false))
	{
		return FRigVMBaseAction::Redo(InController);
	}
#endif
	return false;
}

FRigVMAddRerouteNodeAction::FRigVMAddRerouteNodeAction()
	: PinPath()
	, bShowAsFullNode(false)
	, bAsInput(false)
	, Position(FVector2D::ZeroVector)
	, NodePath()
{
}

FRigVMAddRerouteNodeAction::FRigVMAddRerouteNodeAction(URigVMRerouteNode* InNode, const FString& InPinPath, bool bInAsInput)
	: PinPath(InPinPath)
	, bShowAsFullNode(InNode->GetShowsAsFullNode())
	, bAsInput(bInAsInput)
	, Position(InNode->GetPosition())
	, NodePath(InNode->GetNodePath())
{
}

bool FRigVMAddRerouteNodeAction::Undo(URigVMController* InController)
{
	if (!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->RemoveNodeByName(*NodePath, false);
}

bool FRigVMAddRerouteNodeAction::Redo(URigVMController* InController)
{
#if WITH_EDITOR
	if (URigVMRerouteNode* Node = InController->AddRerouteNodeOnPin(PinPath, bShowAsFullNode, bAsInput, Position, NodePath, false))
	{
		return FRigVMBaseAction::Redo(InController);
	}
#endif
	return false;
}

FRigVMRemoveNodeAction::FRigVMRemoveNodeAction(URigVMNode* InNode)
{
	FRigVMInverseAction InverseAction;

	if (URigVMStructNode* StructNode = Cast<URigVMStructNode>(InNode))
	{
		InverseAction.AddAction(FRigVMAddStructNodeAction(StructNode));
		// todo: add all set pin defaults
	}
	else if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(InNode))
	{
		ensure(false);
		//InverseAction.AddAction(FRigVMAddStructNodeAction(StructNode));
		// todo: add all set pin defaults
	}
	else if (URigVMParameterNode* ParameterNode = Cast<URigVMParameterNode>(InNode))
	{
		ensure(false);
		//InverseAction.AddAction(FRigVMAddStructNodeAction(StructNode));
		// todo: add all set pin defaults
	}
	else if (URigVMCommentNode* CommentNode = Cast<URigVMCommentNode>(InNode))
	{
		ensure(false);
		//InverseAction.AddAction(FRigVMAddStructNodeAction(StructNode));
		// todo: add all set pin defaults
	}
	else if (URigVMRerouteNode* RerouteNode = Cast<URigVMRerouteNode>(InNode))
	{
		ensure(false);
		//InverseAction.AddAction(FRigVMAddStructNodeAction(StructNode));
		// todo: add all set pin defaults
	}
	else
	{
		ensure(false);
	}

	InverseActionKey.Set(InverseAction);
}

bool FRigVMRemoveNodeAction::Undo(URigVMController* InController)
{
	FRigVMActionWrapper InverseWrapper(InverseActionKey);
	if (!InverseWrapper.GetAction()->Undo(InController))
	{
		return false;
	}
	return FRigVMBaseAction::Undo(InController);
}

bool FRigVMRemoveNodeAction::Redo(URigVMController* InController)
{
	if (!FRigVMBaseAction::Redo(InController))
	{
		return false;
	}
	FRigVMActionWrapper InverseWrapper(InverseActionKey);
	return InverseWrapper.GetAction()->Redo(InController);
}

FRigVMSelectNodeAction::FRigVMSelectNodeAction(URigVMNode* InNode)
: NodePath(InNode->GetNodePath())
, bWasSelected(InNode->IsSelected())
{
}

bool FRigVMSelectNodeAction::Undo(URigVMController* InController)
{
	if(!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->SelectNodeByName(*NodePath, bWasSelected, false);
}

bool FRigVMSelectNodeAction::Redo(URigVMController* InController)
{
	if(!InController->SelectNodeByName(*NodePath, !bWasSelected, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo(InController);
}

FRigVMSetNodePositionAction::FRigVMSetNodePositionAction(URigVMNode* InNode, const FVector2D& InNewPosition)
: NodePath(InNode->GetNodePath())
, OldPosition(InNode->GetPosition())
, NewPosition(InNewPosition)
{
}

bool FRigVMSetNodePositionAction::Undo(URigVMController* InController)
{
	if(!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->SetNodePositionByName(*NodePath, OldPosition, false);
}

bool FRigVMSetNodePositionAction::Redo(URigVMController* InController)
{
	if(!InController->SetNodePositionByName(*NodePath, NewPosition, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo(InController);
}

FRigVMSetNodeSizeAction::FRigVMSetNodeSizeAction(URigVMNode* InNode, const FVector2D& InNewSize)
: NodePath(InNode->GetNodePath())
, OldSize(InNode->GetSize())
, NewSize(InNewSize)
{
}

bool FRigVMSetNodeSizeAction::Undo(URigVMController* InController)
{
	if(!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->SetNodeSizeByName(*NodePath, OldSize, false);
}

bool FRigVMSetNodeSizeAction::Redo(URigVMController* InController)
{
	if(!InController->SetNodeSizeByName(*NodePath, NewSize, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo(InController);
}

FRigVMSetCommentTextAction::FRigVMSetCommentTextAction(URigVMCommentNode* InNode, const FString& InNewText)
: NodePath(InNode->GetNodePath())
, OldText(InNode->GetCommentText())
, NewText(InNewText)
{
}

bool FRigVMSetCommentTextAction::Undo(URigVMController* InController)
{
	if(!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->SetCommentTextByName(*NodePath, OldText, false);
}

bool FRigVMSetCommentTextAction::Redo(URigVMController* InController)
{
	if(!InController->SetCommentTextByName(*NodePath, NewText, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo(InController);
}

FRigVMSetRerouteCompactnessAction::FRigVMSetRerouteCompactnessAction(URigVMRerouteNode* InNode, bool InShowAsFullNode)
: NodePath(InNode->GetNodePath())
, OldShowAsFullNode(InNode->GetShowsAsFullNode())
, NewShowAsFullNode(InShowAsFullNode)
{
}

bool FRigVMSetRerouteCompactnessAction::Undo(URigVMController* InController)
{
	if(!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->SetRerouteCompactnessByName(*NodePath, OldShowAsFullNode, false);
}

bool FRigVMSetRerouteCompactnessAction::Redo(URigVMController* InController)
{
	if(!InController->SetRerouteCompactnessByName(*NodePath, NewShowAsFullNode, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo(InController);
}

FRigVMRenameVariableAction::FRigVMRenameVariableAction(const FName& InOldVariableName, const FName& InNewVariableName)
: OldVariableName(InOldVariableName.ToString())
, NewVariableName(InNewVariableName.ToString())
{
}

bool FRigVMRenameVariableAction::Undo(URigVMController* InController)
{
	if(!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->RenameVariable(*NewVariableName, *OldVariableName, false);
}

bool FRigVMRenameVariableAction::Redo(URigVMController* InController)
{
	if(!InController->RenameVariable(*OldVariableName, *NewVariableName, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo(InController);
}

FRigVMRenameParameterAction::FRigVMRenameParameterAction(const FName& InOldParameterName, const FName& InNewParameterName)
: OldParameterName(InOldParameterName.ToString())
, NewParameterName(InNewParameterName.ToString())
{
}

bool FRigVMRenameParameterAction::Undo(URigVMController* InController)
{
	if(!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->RenameParameter(*NewParameterName, *OldParameterName, false);
}

bool FRigVMRenameParameterAction::Redo(URigVMController* InController)
{
	if(!InController->RenameParameter(*OldParameterName, *NewParameterName, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo(InController);
}

FRigVMSetPinDefaultValueAction::FRigVMSetPinDefaultValueAction(URigVMPin* InPin, const FString& InNewDefaultValue)
: PinPath(InPin->GetPinPath())
, OldDefaultValue(InPin->GetDefaultValue())
, NewDefaultValue(InNewDefaultValue)
{
}

bool FRigVMSetPinDefaultValueAction::Undo(URigVMController* InController)
{
	if(!FRigVMBaseAction::Undo(InController))
	{
		return false;
	}
	return InController->SetPinDefaultValue(PinPath, OldDefaultValue, true, false);
}

bool FRigVMSetPinDefaultValueAction::Redo(URigVMController* InController)
{
	if(!InController->SetPinDefaultValue(PinPath, NewDefaultValue, true, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo(InController);
}