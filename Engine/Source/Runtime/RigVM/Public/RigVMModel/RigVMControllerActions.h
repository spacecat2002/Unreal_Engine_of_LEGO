// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/PropertyPortFlags.h"
#include "RigVMController.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif
#include "RigVMControllerActions.generated.h"

struct FRigVMBaseAction;

USTRUCT()
struct FRigVMActionKey
{
	GENERATED_BODY()

	UPROPERTY()
	FString ScriptStructPath;

	UPROPERTY()
	FString ExportedText;

	template<class ActionType>
	void Set(const ActionType& InAction)
	{
		UScriptStruct* ScriptStruct = ActionType::StaticStruct();
		FRigVMActionKey Key;
		ScriptStructPath = ScriptStruct->GetPathName();
		ScriptStruct->ExportText(ExportedText, &InAction, nullptr, nullptr, PPF_None, nullptr);
	}
};

struct FRigVMActionWrapper
{
public:
	FRigVMActionWrapper(const FRigVMActionKey& Key);
	~FRigVMActionWrapper();

	FRigVMBaseAction* GetAction();
	FString ExportText();

private:
	FRigVMActionWrapper(const FRigVMActionWrapper& Other) = delete;
	FRigVMActionWrapper& operator = (const FRigVMActionWrapper& Other) = delete;

	UScriptStruct* ScriptStruct;
	TArray<uint8> Data;
};

USTRUCT()
struct FRigVMBaseAction
{
	GENERATED_BODY()

	FRigVMBaseAction()
		 : Title(TEXT("ACtion"))
	{
	}

	virtual ~FRigVMBaseAction() {};
	virtual FString GetTitle() const { return Title; }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController);
	virtual bool Redo(URigVMController* InController);

	template<class ActionType>
	void AddAction(const ActionType& InAction)
	{
		FRigVMActionKey Key;
		Key.Set<ActionType>(InAction);
		SubActions.Add(Key);
	}

	UPROPERTY()
	FString Title;

	UPROPERTY()
	TArray<FRigVMActionKey> SubActions;
};

UCLASS()
class URigVMActionStack : public UObject
{
	GENERATED_BODY()

public:

	template<class T>
	void BeginAction(T& InAction)
	{
		if (CurrentActions.Num() > 0)
		{
			// catch duplicate calls to begin action
			ensure(CurrentActions.Last() != &InAction);
		}
		CurrentActions.Add((FRigVMBaseAction*)&InAction);
	}

	template<class ActionType>
	void EndAction(ActionType& InAction, bool bPerformMerge = false)
	{
		ensure(CurrentActions.Num() > 0);
		ensure((FRigVMBaseAction*)&InAction == CurrentActions.Last());
		CurrentActions.Pop();
		AddAction(InAction, bPerformMerge);
	}

	template<class ActionType>
	void CancelAction(ActionType& InAction)
	{
		ensure(CurrentActions.Num() > 0);
		ensure((FRigVMBaseAction*)&InAction == CurrentActions.Last());
		CurrentActions.Pop();
	}

	template<class ActionType>
	void AddAction(const ActionType& InAction, bool bPerformMerge = false)
	{
		TArray<FRigVMActionKey>* ActionList = &UndoActions;
		if (CurrentActions.Num() > 0)
		{
			ActionList = &CurrentActions[CurrentActions.Num()-1]->SubActions;
		}

		bool bMergeIfPossible = bPerformMerge;
		if (bMergeIfPossible)
		{
			bMergeIfPossible = false;
			if (ActionList->Num() > 0 && InAction.SubActions.Num() == 0)
			{
				if (ActionList->Last().ScriptStructPath == ActionType::StaticStruct()->GetPathName())
				{
					FRigVMActionWrapper Wrapper((*ActionList)[ActionList->Num() - 1]);
					if (Wrapper.GetAction()->SubActions.Num() == 0)
					{
						bMergeIfPossible = Wrapper.GetAction()->Merge(&InAction);
						if (bMergeIfPossible)
						{
							(*ActionList)[ActionList->Num()-1].ExportedText = Wrapper.ExportText();
						}
					}
				}
			}
		}

		if (!bMergeIfPossible)
		{
			FRigVMActionKey Key;
			Key.Set<ActionType>(InAction);

			ActionList->Add(Key);

			if (CurrentActions.Num() == 0)
			{
				RedoActions.Reset();
			}

#if WITH_EDITOR
			const FScopedTransaction Transaction(FText::FromString(InAction.GetTitle()));
			SetFlags(RF_Transactional);
			Modify();
#endif

			ActionIndex = ActionIndex + 1;
		}
	}

	UFUNCTION()
	bool Undo(URigVMController* InController);

	UFUNCTION()
	bool Redo(URigVMController* InController);

#if WITH_EDITOR
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif

private:

	UPROPERTY()
	int32 ActionIndex;

	UPROPERTY()
	TArray<FRigVMActionKey> UndoActions;

	UPROPERTY()
	TArray<FRigVMActionKey> RedoActions;

	TArray<FRigVMBaseAction*> CurrentActions;
};

USTRUCT()
struct FRigVMInverseAction : public FRigVMBaseAction
{
	GENERATED_BODY()

	virtual ~FRigVMInverseAction() {};
	virtual bool Undo(URigVMController* InController);
	virtual bool Redo(URigVMController* InController);
};

USTRUCT()
struct FRigVMAddStructNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddStructNodeAction();
	FRigVMAddStructNodeAction(URigVMStructNode* InNode);
	virtual ~FRigVMAddStructNodeAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString ScriptStructPath;

	UPROPERTY()
	FName MethodName;
	
	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

USTRUCT()
struct FRigVMAddVariableNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddVariableNodeAction();
	FRigVMAddVariableNodeAction(URigVMVariableNode* InNode);
	virtual ~FRigVMAddVariableNodeAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FName VariableName;
	
	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FString ScriptStructPath;

	UPROPERTY()
	bool bIsGetter;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

USTRUCT()
struct FRigVMAddParameterNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddParameterNodeAction();
	FRigVMAddParameterNodeAction(URigVMParameterNode* InNode);
	virtual ~FRigVMAddParameterNodeAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FName ParameterName;
	
	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FString ScriptStructPath;

	UPROPERTY()
	bool bIsInput;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

USTRUCT()
struct FRigVMAddCommentNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddCommentNodeAction();
	FRigVMAddCommentNodeAction(URigVMCommentNode* InNode);
	virtual ~FRigVMAddCommentNodeAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString CommentText;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FVector2D Size;

	UPROPERTY()
	FString NodePath;
};

USTRUCT()
struct FRigVMAddRerouteNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddRerouteNodeAction();
	FRigVMAddRerouteNodeAction(URigVMRerouteNode* InNode, const FString& InPinPath, bool bInAsInput);
	FRigVMAddRerouteNodeAction(URigVMRerouteNode* InNode);
	virtual ~FRigVMAddRerouteNodeAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinPath;

	UPROPERTY()
	bool bShowAsFullNode;

	UPROPERTY()
	bool bAsInput;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

USTRUCT()
struct FRigVMRemoveNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRemoveNodeAction() {}
	FRigVMRemoveNodeAction(URigVMNode* InNode);
	virtual ~FRigVMRemoveNodeAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FRigVMActionKey InverseActionKey;
};

USTRUCT()
struct FRigVMSelectNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSelectNodeAction() {}
	FRigVMSelectNodeAction(URigVMNode* InNode);
	virtual ~FRigVMSelectNodeAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	bool bWasSelected;
};

USTRUCT()
struct FRigVMSetNodePositionAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodePositionAction()
	{
		OldPosition = NewPosition = FVector2D::ZeroVector;
	}
	FRigVMSetNodePositionAction(URigVMNode* InNode, const FVector2D& InNewPosition);
	virtual ~FRigVMSetNodePositionAction() {};
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FVector2D OldPosition;

	UPROPERTY()
	FVector2D NewPosition;
};

USTRUCT()
struct FRigVMSetNodeSizeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodeSizeAction()
	{
		OldSize = NewSize = FVector2D::ZeroVector;
	}
	FRigVMSetNodeSizeAction(URigVMNode* InNode, const FVector2D& InNewSize);
	virtual ~FRigVMSetNodeSizeAction() {};
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FVector2D OldSize;

	UPROPERTY()
	FVector2D NewSize;
};

USTRUCT()
struct FRigVMSetCommentTextAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetCommentTextAction() {}
	FRigVMSetCommentTextAction(URigVMCommentNode* InNode, const FString& InNewText);
	virtual ~FRigVMSetCommentTextAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FString OldText;

	UPROPERTY()
	FString NewText;
};

USTRUCT()
struct FRigVMSetRerouteCompactnessAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetRerouteCompactnessAction() {}
	FRigVMSetRerouteCompactnessAction(URigVMRerouteNode* InNode, bool InShowAsFullNode);
	virtual ~FRigVMSetRerouteCompactnessAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	bool OldShowAsFullNode;

	UPROPERTY()
	bool NewShowAsFullNode;
};

USTRUCT()
struct FRigVMRenameVariableAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRenameVariableAction() {}
	FRigVMRenameVariableAction(const FName& InOldVariableName, const FName& InNewVariableName);
	virtual ~FRigVMRenameVariableAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString OldVariableName;

	UPROPERTY()
	FString NewVariableName;
};

USTRUCT()
struct FRigVMRenameParameterAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRenameParameterAction() {}
	FRigVMRenameParameterAction(const FName& InOldParameterName, const FName& InNewParameterName);
	virtual ~FRigVMRenameParameterAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString OldParameterName;

	UPROPERTY()
	FString NewParameterName;
};

USTRUCT()
struct FRigVMSetPinDefaultValueAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetPinDefaultValueAction() {}
	FRigVMSetPinDefaultValueAction(URigVMPin* InPin, const FString& InNewDefaultValue);
	virtual ~FRigVMSetPinDefaultValueAction() {};
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinPath;

	UPROPERTY()
	FString OldDefaultValue;

	UPROPERTY()
	FString NewDefaultValue;
};

USTRUCT()
struct FRigVMInsertArrayPinAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMInsertArrayPinAction()
		: Index(0)
	{
	}
	FRigVMInsertArrayPinAction(URigVMPin* InArrayPin, int32 InIndex, const FString& InNewDefaultValue);
	virtual ~FRigVMInsertArrayPinAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString ArrayPinPath;

	UPROPERTY()
	int32 Index;

	UPROPERTY()
	FString NewDefaultValue;
};

USTRUCT()
struct FRigVMRemoveArrayPinAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRemoveArrayPinAction()
		: Index(0)
	{
	}
	FRigVMRemoveArrayPinAction(URigVMPin* InArrayElementPin);
	virtual ~FRigVMRemoveArrayPinAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString ArrayPinPath;

	UPROPERTY()
	int32 Index;

	UPROPERTY()
	FString DefaultValue;
};

USTRUCT()
struct FRigVMAddLinkAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddLinkAction() {}
	FRigVMAddLinkAction(URigVMPin* InOutputPin, URigVMPin* InInputPin);
	virtual ~FRigVMAddLinkAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString OutputPinPath;

	UPROPERTY()
	FString InputPinPath;
};

USTRUCT()
struct FRigVMBreakLinkAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMBreakLinkAction() {}
	FRigVMBreakLinkAction(URigVMPin* InOutputPin, URigVMPin* InInputPin);
	virtual ~FRigVMBreakLinkAction() {};
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString OutputPinPath;

	UPROPERTY()
	FString InputPinPath;
};