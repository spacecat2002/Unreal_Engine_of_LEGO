// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RigVMPin.generated.h"

class URigVMGraph;
class URigVMNode;
class URigVMPin;
class URigVMLink;

UENUM()
enum class ERigVMPinDirection : uint8
{
	Input,
	Output,
	IO,
	Visible,
	Hidden,
	Invalid
};

UCLASS()
class RIGVM_API URigVMPin : public UObject
{
	GENERATED_BODY()

public:

	static bool SplitPinPathAtStart(const FString& InPinPath, FString& LeftMost, FString& Right);
	static bool SplitPinPathAtEnd(const FString& InPinPath, FString& Left, FString& RightMost);
	static bool SplitPinPath(const FString& InPinPath, TArray<FString>& Parts);
	static FString JoinSplitPath(const FString& Left, const FString& Right);
	static FString JoinSplitPath(const TArray<FString>& InParts);

	URigVMPin();

	FString GetPinPath() const;
	ERigVMPinDirection GetDirection() const;
	bool IsConstant() const;
	bool IsStruct() const;
	bool IsStructMember() const;
	bool IsArray() const;
	bool IsArrayElement() const;
	int32 GetIndex() const;
	int32 GetArraySize() const;
	FString GetCPPType() const;
	FString GetArrayElementCppType() const;
	bool IsStringType() const;
	FString GetDefaultValue() const;
	FName GetCustomWidgetName() const;

	UScriptStruct* GetScriptStruct() const;

	URigVMPin* GetParentPin() const;
	const TArray<URigVMPin*>& GetSubPins() const;
	URigVMPin* FindSubPin(const FString& InPinPath) const;
	bool IsConnectedTo(URigVMPin* InPin) const;
	const TArray<URigVMLink*>& GetLinks() const;
	TArray<URigVMPin*> GetConnectedSourcePins() const;
	TArray<URigVMPin*> GetConnectedTargetPins() const;

	URigVMNode* GetNode() const;
	URigVMGraph* GetGraph() const;

	static bool CanLink(URigVMPin* InSourcePin, URigVMPin* InTargetPin, FString* OutFailureReason = nullptr);

private:

	void SetIndex(int32 InNewIndex);

	UPROPERTY()
	ERigVMPinDirection Direction;

	UPROPERTY()
	bool bIsConstant;

	UPROPERTY()
	int32 Index;

	UPROPERTY()
	FString CPPType;

	UPROPERTY(transient)
	UScriptStruct* ScriptStruct;

	UPROPERTY()
	FName ScriptStructPath;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	FName CustomWidgetName;

	UPROPERTY()
	TArray<URigVMPin*> SubPins;

	UPROPERTY(transient)
	TArray<URigVMLink*> Links;

	friend class URigVMController;
	friend class URigVMGraph;
};

