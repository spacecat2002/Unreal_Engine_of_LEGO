// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMController.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMModule.h"
#include "UObject/PropertyPortFlags.h"

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

	FString NamePrefix = InScriptStruct->GetName();
	int32 NameSuffix = 0;
	FString Name = NamePrefix;

	while (!Graph->IsNameAvailable(Name))
	{
		Name = FString::Printf(TEXT("%s_%d"), *NamePrefix, ++NameSuffix);
	}

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

bool URigVMController::RemoveNode(URigVMNode* InNode, bool bUndo)
{
	if (!IsValidNodeForGraph(InNode))
	{
		return false;
	}

	// todo: break all links
	// todo: undo redo

	SelectNode(InNode, false, bUndo);

	Graph->Nodes.Remove(InNode);
	Graph->MarkPackageDirty();

	Notify(ERigVMGraphNotifType::NodeRemoved, Graph, InNode);

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

void URigVMController::SetPinDefaultValue(const FString& InPinPath, const FString& InDefaultValue)
{
	if (Graph)
	{
		URigVMPin* Pin = Graph->FindPin(InPinPath);
		if (Pin)
		{
			if (Pin->Direction == ERigVMPinDirection::Input)
			{
				SetPinDefaultValue(Pin, InDefaultValue);
			}
		}
	}
}

void URigVMController::SetPinDefaultValue(URigVMPin* InPin, const FString& InDefaultValue)
{
	check(InPin);
	ensure(!InDefaultValue.IsEmpty());

	// todo: undo / redo

	if (InPin->IsArray())
	{
		TArray<FString> Elements = SplitDefaultValue(InDefaultValue);
		ensure(Elements.Num() == InPin->SubPins.Num());
		for(int32 ElementIndex=0;ElementIndex<Elements.Num();ElementIndex++)
		{
			URigVMPin* SubPin = InPin->SubPins[ElementIndex];
			if (SubPin->IsStringType())
			{
				Elements[ElementIndex].Mid(1, Elements[ElementIndex].Len() - 2);
			}
			SetPinDefaultValue(SubPin, Elements[ElementIndex]);
		}
	}
	else if (InPin->IsStruct())
	{
		TArray<FString> MemberValuePairs = SplitDefaultValue(InDefaultValue);
		ensure(MemberValuePairs.Num() == InPin->SubPins.Num());
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
					SetPinDefaultValue(SubPin, MemberValue);
				}
			}
		}
	}
	else
	{
		ensure(InPin->GetSubPins().Num() == 0);;
		InPin->DefaultValue = InDefaultValue;
		Graph->Notify(ERigVMGraphNotifType::PinDefaultValueChanged, Graph, InPin);
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
			InParentPin->SubPins.Add(Pin);
		}
		else
		{
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
		FString ElementName = FString::FormatAsNumber(ElementIndex);
		URigVMPin* Pin = NewObject<URigVMPin>(InParentPin, *ElementName);
		ConfigurePinFromProperty(InArrayProperty->Inner, Pin, InPinDirection);

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