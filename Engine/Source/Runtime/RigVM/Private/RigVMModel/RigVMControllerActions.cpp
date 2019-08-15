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
