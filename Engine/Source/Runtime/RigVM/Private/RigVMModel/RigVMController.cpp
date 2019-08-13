// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMController.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMModule.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/Package.h"

int32 URigVMController::sRemovedPinIndex = 0;

URigVMController::~URigVMController()
{
	SetGraph(nullptr);
}

URigVMGraph* URigVMController::GetGraph() const
{
	return Graph;
}

void URigVMController::SetGraph(URigVMGraph* InGraph)
{
	if (Graph)
	{
		Graph->OnModified().RemoveAll(this);
	}

	Graph = InGraph;

	if (Graph)
	{
		Graph->OnModified().AddUObject(this, &URigVMController::HandleModifiedEvent);
	}

	ModifiedEvent.Broadcast(ERigVMGraphNotifType::GraphChanged, Graph, nullptr);
}

FRigVMGraphModifiedEvent& URigVMController::OnModified()
{
	return ModifiedEvent;
}

void URigVMController::Notify(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	if (Graph)
	{
		Graph->Notify(InNotifType, InGraph, InSubject);
	}
}

void URigVMController::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	ModifiedEvent.Broadcast(InNotifType, InGraph, InSubject);
}

URigVMStructNode* URigVMController::AddStructNode(UScriptStruct* InScriptStruct, const FName& InMethodName, const FVector2D& InPosition, bool bUndo)
{
	if (Graph == nullptr)
	{
		return nullptr;
	}

	if (InScriptStruct == nullptr)
	{
		return nullptr;
	}
	if (InMethodName == NAME_None)
	{
		return nullptr;
	}

	FString FunctionName = FString::Printf(TEXT("F%s::%s"), *InScriptStruct->GetName(), *InMethodName.ToString());
	FRigVMFunctionPtr Function = FRigVMRegistry::Get().Find(*FunctionName);
	if (Function == nullptr)
	{
		// todo: log error
		return nullptr;
	}

	FString Name = GetValidName(InScriptStruct->GetName());
	URigVMStructNode* Node = NewObject<URigVMStructNode>(Graph, *Name);
	Node->ScriptStruct = InScriptStruct;
	Node->MethodName = InMethodName;
	Node->Position = InPosition;
	
	TArray<uint8> TempBuffer;
	TempBuffer.AddUninitialized(InScriptStruct->GetStructureSize());
	InScriptStruct->InitializeDefaultValue(TempBuffer.GetData());

	FString ExportedDefaultValue;
	InScriptStruct->ExportText(ExportedDefaultValue, TempBuffer.GetData(), nullptr, nullptr, PPF_None, nullptr);
	InScriptStruct->DestroyStruct(TempBuffer.GetData());
	AddPinsForStruct(InScriptStruct, Node, nullptr, ERigVMPinDirection::Invalid, ExportedDefaultValue);

	Graph->Nodes.Add(Node);
	Graph->MarkPackageDirty();

	// todo: undo redo

	Notify(ERigVMGraphNotifType::NodeAdded, Graph, Node);

	return Node;
}

URigVMCommentNode* URigVMController::AddCommentNode(const FString& InCommentText, const FVector2D& InPosition, const FVector2D& InSize, bool bUndo)
{
	if (Graph == nullptr)
	{
		return nullptr;
	}

	FString Name = GetValidName(TEXT("CommentNode"));
	URigVMCommentNode* Node = NewObject<URigVMCommentNode>(Graph, *Name);
	Node->Position = InPosition;
	Node->Size = InSize;
	Node->CommentText = InCommentText;

	Graph->Nodes.Add(Node);
	Graph->MarkPackageDirty();

	// todo: undo redo

	Notify(ERigVMGraphNotifType::NodeAdded, Graph, Node);

	return Node;
}

URigVMRerouteNode* URigVMController::AddRerouteNodeOnLink(URigVMLink* InLink, bool bShowAsFullNode, const FVector2D& InPosition, bool bUndo)
{
	if(!IsValidLinkForGraph(InLink))
	{
		return nullptr;
	}

	URigVMPin* SourcePin = InLink->GetSourcePin();
	URigVMPin* TargetPin = InLink->GetTargetPin();

	// todo: undo redo

	URigVMRerouteNode* Node = AddRerouteNodeOnPin(TargetPin->GetPinPath(), true, bShowAsFullNode, InPosition, bUndo);
	if (Node == nullptr)
	{
		return nullptr;
	}

	URigVMPin* ValuePin = Node->Pins[0];
	AddLink(SourcePin, ValuePin, bUndo);

	return Node;
}

URigVMRerouteNode* URigVMController::AddRerouteNodeOnPin(const FString& InPinPath, bool bAsInput, bool bShowAsFullNode, const FVector2D& InPosition, bool bUndo)
{
	if (Graph == nullptr)
	{
		return nullptr;
	}

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if(Pin == nullptr)
	{
		return nullptr;
	}

	// todo: undo redo

	BreakAllLinks(Pin, bAsInput, bUndo);

	FString Name = GetValidName(TEXT("RerouteNode"));
	URigVMRerouteNode* Node = NewObject<URigVMRerouteNode>(Graph, *Name);
	Node->Position = InPosition;
	Node->bShowAsFullNode = bShowAsFullNode;

	URigVMPin* ValuePin = NewObject<URigVMPin>(Node, TEXT("Value"));
	ConfigurePinFromPin(ValuePin, Pin);
	ValuePin->Index = 0;
	ValuePin->Direction = ERigVMPinDirection::IO;
	Node->Pins.Add(ValuePin);

	if (ValuePin->IsStruct())
	{
		AddPinsForStruct(ValuePin->GetScriptStruct(), Node, ValuePin, ValuePin->Direction, FString());
	}

	FString DefaultValue = Pin->GetDefaultValue();
	if (!DefaultValue.IsEmpty())
	{
		SetPinDefaultValue(ValuePin, Pin->GetDefaultValue(), true, false);
	}

	Graph->Nodes.Add(Node);
	Graph->MarkPackageDirty();

	Notify(ERigVMGraphNotifType::NodeAdded, Graph, Node);

	if (bAsInput)
	{
		AddLink(ValuePin, Pin, bUndo);
	}
	else
	{
		AddLink(Pin, ValuePin, bUndo);
	}

	return Node;
}

bool URigVMController::RemoveNode(URigVMNode* InNode, bool bUndo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	// todo: undo redo

	SelectNode(InNode, false, bUndo);

	for (URigVMPin* Pin : InNode->Pins)
	{
		BreakAllLinks(Pin, true, bUndo);
		BreakAllLinks(Pin, false, bUndo);
		BreakAllLinksRecursive(Pin, true, false, bUndo);
		BreakAllLinksRecursive(Pin, false, false, bUndo);
	}

	Graph->Nodes.Remove(InNode);
	Graph->MarkPackageDirty();

	Notify(ERigVMGraphNotifType::NodeRemoved, Graph, InNode);

	InNode->MarkPendingKill();
	return true;
}

bool URigVMController::RemoveNode(const FName& InNodeName, bool bUndo)
{
	if (Graph == nullptr)
	{
		return false;
	}
	return RemoveNode(Graph->FindNode(InNodeName), bUndo);
}

bool URigVMController::SelectNode(URigVMNode* InNode, bool bSelect, bool bUndo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if (InNode->IsSelected() == bSelect)
	{
		return false;
	}

	if (bSelect)
	{
		Graph->SelectedNodes.Add(InNode->GetFName());
		Notify(ERigVMGraphNotifType::NodeSelected, Graph, InNode);
	}
	else
	{
		Graph->SelectedNodes.Remove(InNode->GetFName());
		Notify(ERigVMGraphNotifType::NodeDeselected, Graph, InNode);
	}

	return true;
}

bool URigVMController::SelectNode(const FName& InNodeName, bool bSelect, bool bUndo)
{
	if (Graph == nullptr)
	{
		return false;
	}
	return SelectNode(Graph->FindNode(InNodeName), bSelect, bUndo);
}

bool URigVMController::ClearNodeSelection(bool bUndo)
{
	if (Graph == nullptr)
	{
		return false;
	}

	TArray<FName> Selection = Graph->SelectedNodes;
	for (const FName& SelectedNode : Selection)
	{
		SelectNode(SelectedNode, false, bUndo);
	}

	return Selection.Num() > 0;
}

bool URigVMController::SetNodePosition(URigVMNode* InNode, const FVector2D& InPosition, bool bUndo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	InNode->Position = InPosition;
	// todo: undo redo
	Notify(ERigVMGraphNotifType::NodePositionChanged, Graph, InNode);
	return true;
}

bool URigVMController::SetNodeSize(URigVMNode* InNode, const FVector2D& InSize, bool bUndo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	InNode->Size = InSize;
	// todo: undo redo
	Notify(ERigVMGraphNotifType::NodeSizeChanged, Graph, InNode);
	return true;
}

bool URigVMController::SetCommentText(URigVMNode* InNode, const FString& InCommentText, bool bUndo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if (URigVMCommentNode* CommentNode = Cast<URigVMCommentNode>(InNode))
	{
		CommentNode->CommentText = InCommentText;
		// todo: undo redo
		Notify(ERigVMGraphNotifType::CommentTextChanged, Graph, InNode);
		return true;
	}

	return false;
}

bool URigVMController::SetRerouteCompactness(URigVMNode* InNode, bool bShowAsFullNode, bool bUndo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	if (URigVMRerouteNode* RerouteNode = Cast<URigVMRerouteNode>(InNode))
	{
		RerouteNode->bShowAsFullNode = bShowAsFullNode;
		// todo: undo redo
		Notify(ERigVMGraphNotifType::RerouteCompactnessChanged, Graph, InNode);
		return true;
	}

	return false;
}

FString URigVMController::GetPinDefaultValue(const FString& InPinPath)
{
	if (Graph)
	{
		URigVMPin* Pin = Graph->FindPin(InPinPath);
		if (Pin)
		{
			return Pin->GetDefaultValue();
		}
	}
	return FString();
}

void URigVMController::SetPinDefaultValue(const FString& InPinPath, const FString& InDefaultValue, bool bUndo)
{
	if (Graph)
	{
		URigVMPin* Pin = Graph->FindPin(InPinPath);
		if (Pin)
		{
			if (Pin->Direction == ERigVMPinDirection::Input)
			{
				SetPinDefaultValue(Pin, InDefaultValue, false, bUndo);
			}
		}
	}
}

void URigVMController::SetPinDefaultValue(URigVMPin* InPin, const FString& InDefaultValue, bool bResizeArrays, bool bUndo)
{
	check(InPin);
	ensure(!InDefaultValue.IsEmpty());

	// todo: undo / redo

	if (InPin->IsArray())
	{
		TArray<FString> Elements = SplitDefaultValue(InDefaultValue);

		if (bResizeArrays)
		{
			while (Elements.Num() > InPin->SubPins.Num())
			{
				InsertArrayPin(InPin, INDEX_NONE, FString(), bUndo);
			}
			while (Elements.Num() < InPin->SubPins.Num())
			{
				RemovePin(InPin->SubPins.Last(), bUndo);
			}
		}
		else
		{
			ensure(Elements.Num() == InPin->SubPins.Num());
		}

		for(int32 ElementIndex=0;ElementIndex<Elements.Num();ElementIndex++)
		{
			URigVMPin* SubPin = InPin->SubPins[ElementIndex];
			if (SubPin->IsStringType())
			{
				Elements[ElementIndex].Mid(1, Elements[ElementIndex].Len() - 2);
			}
			SetPinDefaultValue(SubPin, Elements[ElementIndex], bResizeArrays, bUndo);
		}
	}
	else if (InPin->IsStruct())
	{
		TArray<FString> MemberValuePairs = SplitDefaultValue(InDefaultValue);

		for (const FString& MemberValuePair : MemberValuePairs)
		{
			FString MemberName, MemberValue;
			if (MemberValuePair.Split(TEXT("="), &MemberName, &MemberValue))
			{
				URigVMPin* SubPin = InPin->FindSubPin(MemberName);
				if (SubPin)
				{
					if (SubPin->IsStringType())
					{
						MemberValue.Mid(1, MemberValue.Len() - 2);
					}
					SetPinDefaultValue(SubPin, MemberValue, bResizeArrays, bUndo);
				}
			}
		}
	}
	else
	{
		// todo: undo redo
		ensure(InPin->GetSubPins().Num() == 0);;
		InPin->DefaultValue = InDefaultValue;
		Graph->Notify(ERigVMGraphNotifType::PinDefaultValueChanged, Graph, InPin);
		Graph->MarkPackageDirty();
	}
}

TArray<FString> URigVMController::SplitDefaultValue(const FString& InDefaultValue)
{
	TArray<FString> Parts;
	if (InDefaultValue.IsEmpty())
	{
		return Parts;
	}

	ensure(InDefaultValue[0] == TCHAR('('));
	ensure(InDefaultValue[InDefaultValue.Len() - 1] == TCHAR(')'));

	FString Content = InDefaultValue.Mid(1, InDefaultValue.Len() - 2);
	int32 BraceCount = 0;
	int32 QuoteCount = 0;

	int32 LastPartStartIndex = 0;
	for (int32 CharIndex = 0; CharIndex < Content.Len(); CharIndex++)
	{
		TCHAR Char = Content[CharIndex];
		if (QuoteCount > 0)
		{
			if (Content[CharIndex - 1] == TCHAR('\\') && Char == TCHAR('"'))
			{
				QuoteCount = 0;
			}
		}
		else if (Char == TCHAR('"'))
		{
			QuoteCount = 1;
		}
		else if (Char == TCHAR('('))
		{
			BraceCount++;
		}
		else if (Char == TCHAR(')'))
		{
			BraceCount--;
			BraceCount = FMath::Max<int32>(BraceCount, 0);
		}
		else if (Char == TCHAR(',') && BraceCount == 0)
		{
			Parts.Add(Content.Mid(LastPartStartIndex, CharIndex - LastPartStartIndex));
			LastPartStartIndex = CharIndex + 1;
		}
	}

	Parts.Add(Content.Mid(LastPartStartIndex));
	return Parts;
}

FString URigVMController::AddArrayPin(const FString& InArrayPinPath, const FString& InDefaultValue, bool bUndo)
{
	return InsertArrayPin(InArrayPinPath, INDEX_NONE, InDefaultValue, bUndo);
}

FString URigVMController::DuplicateArrayPin(const FString& InArrayElementPinPath, bool bUndo)
{
	if (Graph == nullptr)
	{
		return FString();
	}

	URigVMPin* ElementPin = Graph->FindPin(InArrayElementPinPath);
	if (ElementPin == nullptr)
	{
		return FString();
	}

	if (!ElementPin->IsArrayElement())
	{
		return FString();
	}

	URigVMPin* ArrayPin = ElementPin->GetParentPin();
	check(ArrayPin);
	ensure(ArrayPin->IsArray());

	FString DefaultValue = ElementPin->GetDefaultValue();
	return InsertArrayPin(ArrayPin->GetPinPath(), ElementPin->Index + 1, DefaultValue, bUndo);
}

FString URigVMController::InsertArrayPin(const FString& InArrayPinPath, int32 InIndex, const FString& InDefaultValue, bool bUndo)
{
	if (Graph == nullptr)
	{
		return FString();
	}

	URigVMPin* ArrayPin = Graph->FindPin(InArrayPinPath);
	if (ArrayPin == nullptr)
	{
		return FString();
	}

	URigVMPin* ElementPin = InsertArrayPin(ArrayPin, InIndex, InDefaultValue, bUndo);
	if (ElementPin)
	{
		return ElementPin->GetPinPath();
	}

	return FString();
}

URigVMPin* URigVMController::InsertArrayPin(URigVMPin* ArrayPin, int32 InIndex, const FString& InDefaultValue, bool bUndo)
{
	if (!ArrayPin->IsArray())
	{
		return nullptr;
	}

	if (InIndex == INDEX_NONE)
	{
		InIndex = ArrayPin->GetSubPins().Num();
	}

	for (int32 ExistingIndex = ArrayPin->GetSubPins().Num() - 1; ExistingIndex >= InIndex; ExistingIndex--)
	{
		URigVMPin* ExistingPin = ArrayPin->GetSubPins()[ExistingIndex];
		ExistingPin->Index++;
		ExistingPin->Rename(*FString::FormatAsNumber(ExistingPin->Index));
	}

	// todo: undo redo

	URigVMPin* Pin = NewObject<URigVMPin>(ArrayPin, *FString::FormatAsNumber(InIndex));
	ConfigurePinFromPin(Pin, ArrayPin);
	Pin->Index = InIndex;
	Pin->CPPType = ArrayPin->GetArrayElementCppType();
	ArrayPin->SubPins.Insert(Pin, InIndex);

	if (Pin->IsStruct())
	{
		UScriptStruct* ScriptStruct = Pin->GetScriptStruct();
		if (ScriptStruct)
		{
			AddPinsForStruct(ScriptStruct, Pin->GetNode(), Pin, Pin->Direction, InDefaultValue);
		}
	}
	else if (Pin->IsArray())
	{
		UArrayProperty * ArrayProperty = Cast<UArrayProperty>(FindPropertyForPin(Pin->GetPinPath()));
		if (ArrayProperty)
		{
			TArray<FString> ElementDefaultValues = SplitDefaultValue(InDefaultValue);
			AddPinsForArray(ArrayProperty, Pin->GetNode(), Pin, Pin->Direction, ElementDefaultValues);
		}
	}
	else
	{
		Pin->DefaultValue = InDefaultValue;
	}

	Graph->Notify(ERigVMGraphNotifType::PinArraySizeChanged, Graph, ArrayPin);
	Graph->MarkPackageDirty();

	return Pin;
}

bool URigVMController::RemoveArrayPin(const FString& InArrayElementPinPath, bool bUndo)
{
	if (Graph == nullptr)
	{
		return false;
	}

	URigVMPin* ArrayElementPin = Graph->FindPin(InArrayElementPinPath);
	if (ArrayElementPin == nullptr)
	{
		return false;
	}

	if (!ArrayElementPin->IsArrayElement())
	{
		return false;
	}

	URigVMPin* ArrayPin = ArrayElementPin->GetParentPin();
	check(ArrayPin);
	ensure(ArrayPin->IsArray());

	// todo: undo redo

	int32 IndexToRemove = ArrayElementPin->Index;
	if (!RemovePin(ArrayElementPin, bUndo))
	{
		return false;
	}

	for (int32 ExistingIndex = ArrayPin->GetSubPins().Num() - 1; ExistingIndex >= IndexToRemove; ExistingIndex--)
	{
		URigVMPin* ExistingPin = ArrayPin->GetSubPins()[ExistingIndex];
		ExistingPin->SetIndex(ExistingIndex);
	}

	Graph->MarkPackageDirty();
	Graph->Notify(ERigVMGraphNotifType::PinArraySizeChanged, Graph, ArrayPin);
	return true;
}

bool URigVMController::RemovePin(URigVMPin* InPinToRemove, bool bUndo)
{
	BreakAllLinks(InPinToRemove, true, bUndo);
	BreakAllLinks(InPinToRemove, false, bUndo);
	BreakAllLinksRecursive(InPinToRemove, true, false, bUndo);
	BreakAllLinksRecursive(InPinToRemove, false, false, bUndo);

	URigVMPin* ParentPin = InPinToRemove->GetParentPin();
	ParentPin->SubPins.Remove(InPinToRemove);
	InPinToRemove->Rename(*FString::Printf(TEXT("URigVMPin_%d_Removed"), ++sRemovedPinIndex), GetTransientPackage());

	TArray<URigVMPin*> SubPins = InPinToRemove->GetSubPins();
	for (URigVMPin* SubPin : SubPins)
	{
		if (!RemovePin(SubPin, bUndo))
		{
			return false;
		}
	}

	InPinToRemove->MarkPendingKill();
	return true;
}

bool URigVMController::ClearArrayPin(const FString& InArrayPinPath, bool bUndo)
{
	if (Graph == nullptr)
	{
		return false;
	}

	URigVMPin* Pin = Graph->FindPin(InArrayPinPath);
	if (Pin == nullptr)
	{
		return false;
	}

	if (!Pin->IsArray())
	{
		return false;
	}

	// todo: undo redo

	TArray<URigVMPin*> ElementPins = Pin->GetSubPins();
	for (int32 ElementIndex = ElementPins.Num() - 1; ElementIndex >= 0; ElementIndex--)
	{
		if (!RemoveArrayPin(ElementPins[ElementIndex]->GetPinPath(), bUndo))
		{
			return false;
		}
	}

	return ElementPins.Num() > 0;
}

bool URigVMController::AddLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bUndo)
{
	if (Graph == nullptr)
	{
		return false;
	}

	URigVMPin* OutputPin = Graph->FindPin(InOutputPinPath);
	URigVMPin* InputPin = Graph->FindPin(InInputPinPath);
	if (OutputPin == nullptr || InputPin == nullptr)
	{
		return false;
	}

	return AddLink(OutputPin, InputPin, bUndo);
}

bool URigVMController::AddLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bUndo)
{
	if (!Graph->CanLink(OutputPin, InputPin))
	{
		return false;
	}

	if (OutputPin->IsConnectedTo(InputPin))
	{
		return false;
	}
	ensure(!InputPin->IsConnectedTo(OutputPin));

	BreakAllLinks(InputPin, true, bUndo);
	BreakAllLinksRecursive(InputPin, true, true, bUndo);
	BreakAllLinksRecursive(InputPin, true, false, bUndo);

	URigVMLink* Link = NewObject<URigVMLink>(Graph);
	Link->SourcePin = OutputPin;
	Link->TargetPin = InputPin;
	Link->SourcePinPath = OutputPin->GetPinPath();
	Link->TargetPinPath = InputPin->GetPinPath();
	Graph->Links.Add(Link);
	OutputPin->Links.Add(Link);
	InputPin->Links.Add(Link);

	Graph->MarkPackageDirty();
	Graph->Notify(ERigVMGraphNotifType::LinkAdded, Graph, Link);

	// todo: undo redo

	return true;
}

bool URigVMController::BreakLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bUndo)
{
	if (Graph == nullptr)
	{
		return false;
	}

	URigVMPin* OutputPin = Graph->FindPin(InOutputPinPath);
	URigVMPin* InputPin = Graph->FindPin(InInputPinPath);
	if (OutputPin == nullptr || InputPin == nullptr)
	{
		return false;
	}

	return BreakLink(OutputPin, InputPin, bUndo);
}

bool URigVMController::BreakLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bUndo)
{
	if (!OutputPin->IsConnectedTo(InputPin))
	{
		return false;
	}
	ensure(InputPin->IsConnectedTo(OutputPin));

	for (URigVMLink* Link : InputPin->Links)
	{
		if (Link->SourcePin == OutputPin && Link->TargetPin == InputPin)
		{
			OutputPin->Links.Remove(Link);
			InputPin->Links.Remove(Link);
			Graph->Links.Remove(Link);
			
			Graph->MarkPackageDirty();
			Graph->Notify(ERigVMGraphNotifType::LinkRemoved, Graph, Link);

			Link->MarkPendingKill();

			// todo: undo redo

			return true;
		}
	}

	return false;
}

bool URigVMController::BreakAllLinks(const FString& InPinPath, bool bAsInput, bool bUndo)
{
	if (Graph == nullptr)
	{
		return false;
	}

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		return false;
	}

	return BreakAllLinks(Pin, bAsInput, bUndo);
}

bool URigVMController::BreakAllLinks(URigVMPin* Pin, bool bAsInput, bool bUndo)
{
	int32 LinksBroken = 0;
	TArray<URigVMLink*> Links = Pin->GetLinks();
	for (int32 LinkIndex = Links.Num() - 1; LinkIndex >= 0; LinkIndex--)
	{
		URigVMLink* Link = Links[LinkIndex];
		if (bAsInput && Link->GetTargetPin() == Pin)
		{
			LinksBroken += BreakLink(Link->GetSourcePin(), Pin, bUndo) ? 1 : 0;
		}
		else if (!bAsInput && Link->GetSourcePin() == Pin)
		{
			LinksBroken += BreakLink(Pin, Link->GetTargetPin(), bUndo) ? 1 : 0;
		}
	}

	// todo: undo redo

	return LinksBroken > 0;
}

void URigVMController::BreakAllLinksRecursive(URigVMPin* Pin, bool bAsInput, bool bTowardsParent, bool bUndo)
{
	if (bTowardsParent)
	{
		URigVMPin* ParentPin = Pin->GetParentPin();
		if (ParentPin)
		{
			BreakAllLinks(ParentPin, bAsInput, bUndo);
			BreakAllLinksRecursive(ParentPin, bAsInput, bTowardsParent, bUndo);
		}
	}
	else
	{
		for (URigVMPin* SubPin : Pin->SubPins)
		{
			BreakAllLinks(SubPin, bAsInput, bUndo);
			BreakAllLinksRecursive(SubPin, bAsInput, bTowardsParent, bUndo);
		}
	}
}

FString URigVMController::GetValidName(const FString& InPrefix)
{
	int32 NameSuffix = 0;
	FString Name = InPrefix;

	while (!Graph->IsNameAvailable(Name))
	{
		Name = FString::Printf(TEXT("%s_%d"), *InPrefix, ++NameSuffix);
	}

	return Name;
}

bool URigVMController::IsValidNodeForGraph(URigVMNode* InNode)
{
	if (Graph == nullptr)
	{
		return false;
	}

	if (InNode == nullptr)
	{
		return false;
	}

	if (InNode->GetGraph() != Graph)
	{
		return false;
	}

	return true;
}

bool URigVMController::IsValidLinkForGraph(URigVMLink* InLink)
{
	if (Graph == nullptr)
	{
		return false;
	}

	if (InLink == nullptr)
	{
		return false;
	}

	if (InLink->GetGraph() != Graph)
	{
		return false;
	}

	if(InLink->GetSourcePin() == nullptr)
	{
		return false;
	}

	if(InLink->GetTargetPin() == nullptr)
	{
		return false;
	}

	return true;
}

void URigVMController::AddPinsForStruct(UStruct* InStruct, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const FString& InDefaultValue)
{
	TArray<FString> MemberNameValuePairs = SplitDefaultValue(InDefaultValue);
	TMap<FName, FString> MemberValues;
	for (const FString& MemberNameValuePair : MemberNameValuePairs)
	{
		FString MemberName, MemberValue;
		if (MemberNameValuePair.Split(TEXT("="), &MemberName, &MemberValue))
		{
			MemberValues.Add(*MemberName, MemberValue);
		}
	}

	for (TFieldIterator<UProperty> It(InStruct); It; ++It)
	{
		FName PropertyName = It->GetFName();

		URigVMPin* Pin = NewObject<URigVMPin>(InParentPin == nullptr ? Cast<UObject>(InNode) : Cast<UObject>(InParentPin), PropertyName);
		ConfigurePinFromProperty(*It, Pin, InPinDirection);

		if (InParentPin)
		{
			Pin->Index = InParentPin->SubPins.Num();
			InParentPin->SubPins.Add(Pin);
		}
		else
		{
			Pin->Index = InNode->Pins.Num();
			InNode->Pins.Add(Pin);
		}

		FString* DefaultValuePtr = MemberValues.Find(Pin->GetFName());

		UStructProperty* StructProperty = Cast<UStructProperty>(*It);
		if (StructProperty)
		{
			if (ShouldStructBeUnfolded(StructProperty->Struct))
			{
				FString DefaultValue;
				if (DefaultValuePtr != nullptr)
				{
					DefaultValue = *DefaultValuePtr;
				}
				AddPinsForStruct(StructProperty->Struct, InNode, Pin, Pin->GetDirection(), DefaultValue);
			}
		}

		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(*It);
		if (ArrayProperty)
		{
			ensure(Pin->IsArray());

			if (DefaultValuePtr)
			{
				TArray<FString> ElementDefaultValues = SplitDefaultValue(*DefaultValuePtr);
				AddPinsForArray(ArrayProperty, InNode, Pin, Pin->Direction, ElementDefaultValues);
			}
		}
		
		if (!Pin->IsArray() && !Pin->IsStruct() && DefaultValuePtr != nullptr)
		{
			FString DefaultValue = *DefaultValuePtr;
			if (Pin->IsStringType())
			{
				DefaultValue = DefaultValue.Mid(1, DefaultValue.Len() - 2);
			}
			Pin->DefaultValue = DefaultValue;
		}
	}
}

void URigVMController::AddPinsForArray(UArrayProperty* InArrayProperty, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const TArray<FString>& InDefaultValues)
{
	for (int32 ElementIndex = 0; ElementIndex < InDefaultValues.Num(); ElementIndex++)
	{
		FString ElementName = FString::FormatAsNumber(InParentPin->SubPins.Num());
		URigVMPin* Pin = NewObject<URigVMPin>(InParentPin, *ElementName);
		ConfigurePinFromProperty(InArrayProperty->Inner, Pin, InPinDirection);
		Pin->Index = InParentPin->SubPins.Num();

		InParentPin->SubPins.Add(Pin);

		UStructProperty* StructProperty = Cast<UStructProperty>(InArrayProperty->Inner);
		if (StructProperty)
		{
			AddPinsForStruct(StructProperty->Struct, InNode, Pin, Pin->Direction, InDefaultValues[ElementIndex]);
		}

		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(InArrayProperty->Inner);
		if (ArrayProperty)
		{
			TArray<FString> ElementDefaultValues = SplitDefaultValue(InDefaultValues[ElementIndex]);
			AddPinsForArray(ArrayProperty, InNode, Pin, Pin->Direction, ElementDefaultValues);
		}

		if (!Pin->IsArray() && !Pin->IsStruct())
		{
			FString DefaultValue = InDefaultValues[ElementIndex];
			if (Pin->IsStringType())
			{
				DefaultValue = DefaultValue.Mid(1, DefaultValue.Len() - 2);
			}
			Pin->DefaultValue = DefaultValue;
		}
	}
}

void URigVMController::ConfigurePinFromProperty(UProperty* InProperty, URigVMPin* InOutPin, ERigVMPinDirection InPinDirection)
{
	if (InPinDirection == ERigVMPinDirection::Invalid)
	{
		bool bIsInput = InProperty->HasMetaData(TEXT("Input"));
		bool bIsOutput = InProperty->HasMetaData(TEXT("Output"));

		if (bIsInput)
		{
			InOutPin->Direction = bIsOutput ? ERigVMPinDirection::IO : ERigVMPinDirection::Input;
		}
		else
		{
			InOutPin->Direction = bIsOutput ? ERigVMPinDirection::Output : ERigVMPinDirection::Hidden;
		}
	}
	else
	{
		InOutPin->Direction = InPinDirection;
	}
	InOutPin->bIsConstant = InProperty->HasMetaData(TEXT("Constant"));

	FString ExtendedCppType;
	InOutPin->CPPType = InProperty->GetCPPType(&ExtendedCppType);
	InOutPin->CPPType += ExtendedCppType;

	UStructProperty* StructProperty = Cast<UStructProperty>(InProperty);
	if (StructProperty)
	{
		InOutPin->ScriptStruct = StructProperty->Struct;
		InOutPin->ScriptStructPath = *InOutPin->ScriptStruct->GetPathName();
	}
}

void URigVMController::ConfigurePinFromPin(URigVMPin* InOutPin, URigVMPin* InPin)
{
	InOutPin->bIsConstant = InPin->bIsConstant;
	InOutPin->Direction = InPin->Direction;
	InOutPin->CPPType = InPin->CPPType;
	InOutPin->ScriptStructPath = InPin->ScriptStructPath;
	InOutPin->DefaultValue = InPin->DefaultValue;
}

bool URigVMController::ShouldStructBeUnfolded(const UStruct* Struct)
{
	if (Struct == nullptr)
	{
		return false;
	}
	if (Struct->IsChildOf(UClass::StaticClass()))
	{
		return false;
	}

	return true;
}

UProperty* URigVMController::FindPropertyForPin(const FString& InPinPath)
{
	if (Graph == nullptr)
	{
		return nullptr;
	}

	TArray<FString> Parts;
	if (!URigVMPin::SplitPinPath(InPinPath, Parts))
	{
		return nullptr;
	}

	URigVMPin* Pin = Graph->FindPin(InPinPath);
	if (Pin == nullptr)
	{
		return nullptr;
	}

	URigVMNode* Node = Pin->GetNode();

	URigVMStructNode* StructNode = Cast<URigVMStructNode>(Node);
	if (StructNode)
	{
		int32 PartIndex = 1; // cut off the first one since it's the node

		UStruct* Struct = StructNode->ScriptStruct;
		UProperty* Property = Struct->FindPropertyByName(*Parts[PartIndex++]);

		while (PartIndex < Parts.Num() && Property != nullptr)
		{
			if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
			{
				Property = ArrayProperty->Inner;
				PartIndex++;
				continue;
			}

			if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
			{
				Struct = StructProperty->Struct;
				Property = Struct->FindPropertyByName(*Parts[PartIndex++]);
				continue;
			}

			break;
		}

		if (PartIndex == Parts.Num())
		{
			return Property;
		}
	}

	return nullptr;
}

void URigVMController::DetachLinksFromPinObjects()
{
	check(Graph);

	for (URigVMLink* Link : Graph->Links)
	{
		URigVMPin* SourcePin = Link->GetSourcePin();
		URigVMPin* TargetPin = Link->GetTargetPin();

		Link->SourcePinPath = SourcePin->GetPinPath();
		Link->TargetPinPath = TargetPin->GetPinPath();
		Link->SourcePin = nullptr;
		Link->TargetPin = nullptr;
	}
}

void URigVMController::ReattachLinksToPinObjects()
{
	check(Graph);

	// fix up the pin links based on the persisted data
	TArray<URigVMLink*> NewLinks;
	for (URigVMLink* Link : Graph->Links)
	{
		URigVMPin* SourcePin = Link->GetSourcePin();
		URigVMPin* TargetPin = Link->GetTargetPin();
		if (SourcePin == nullptr)
		{
			if (TargetPin != nullptr)
			{
				TargetPin->Links.Remove(Link);
			}
			continue;
		}
		if (SourcePin == nullptr)
		{
			if (SourcePin != nullptr)
			{
				SourcePin->Links.Remove(Link);
			}
			continue;
		}

		SourcePin->Links.AddUnique(Link);
		TargetPin->Links.AddUnique(Link);
		NewLinks.Add(Link);
	}
	Graph->Links = NewLinks;
}

void URigVMController::RepopulatePinsOnNode(URigVMNode* InNode)
{
	if (InNode == nullptr)
	{
		return;
	}

	check(Graph);

	TMap<FName, FString> DefaultValues;
	for (URigVMPin* Pin : InNode->Pins)
	{
		FString DefaultValue = Pin->GetDefaultValue();
		if (!DefaultValue.IsEmpty())
		{
			DefaultValues.Add(Pin->GetFName(), DefaultValue);
		}
	}

	if (URigVMStructNode* StructNode = Cast<URigVMStructNode>(InNode))
	{
		StructNode->Pins.Reset();

		UScriptStruct* ScriptStruct = StructNode->GetScriptStruct();

		TArray<uint8> TempBuffer;
		TempBuffer.AddUninitialized(ScriptStruct->GetStructureSize());
		ScriptStruct->InitializeDefaultValue(TempBuffer.GetData());

		FString ExportedDefaultValue;
		ScriptStruct->ExportText(ExportedDefaultValue, TempBuffer.GetData(), nullptr, nullptr, PPF_None, nullptr);
		ScriptStruct->DestroyStruct(TempBuffer.GetData());
		AddPinsForStruct(ScriptStruct, StructNode, nullptr, ERigVMPinDirection::Invalid, ExportedDefaultValue);
	}
	else
	{
		return;
	}

	for (URigVMPin* Pin : InNode->Pins)
	{
		FString* DefaultValue = DefaultValues.Find(Pin->GetFName());
		if (DefaultValue)
		{
			SetPinDefaultValue(Pin, *DefaultValue, true, false);
		}
	}
}
