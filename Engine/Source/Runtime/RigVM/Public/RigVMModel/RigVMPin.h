// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RigVMPin.generated.h"

class URigVMGraph;
class URigVMNode;
class URigVMPin;
class URigVMLink;

UENUM(BlueprintType)
enum class ERigVMPinDirection : uint8
{
	Input,
	Output,
	IO,
	Visible,
	Hidden,
	Invalid
};

UCLASS(BlueprintType)
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

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetPinPath() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	ERigVMPinDirection GetDirection() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsConstant() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsStruct() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsStructMember() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsArray() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsArrayElement() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	int32 GetPinIndex() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	int32 GetArraySize() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetCPPType() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetArrayElementCppType() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsStringType() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetDefaultValue() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FName GetCustomWidgetName() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	UScriptStruct* GetScriptStruct() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMPin* GetParentPin() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	const TArray<URigVMPin*>& GetSubPins() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMPin* FindSubPin(const FString& InPinPath) const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsConnectedTo(URigVMPin* InPin) const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	const TArray<URigVMLink*>& GetLinks() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	TArray<URigVMPin*> GetConnectedSourcePins() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	TArray<URigVMPin*> GetConnectedTargetPins() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMNode* GetNode() const;

	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMGraph* GetGraph() const;

	static bool CanLink(URigVMPin* InSourcePin, URigVMPin* InTargetPin, FString* OutFailureReason = nullptr);

private:

	void SetNameFromIndex();

	UPROPERTY()
	ERigVMPinDirection Direction;

	UPROPERTY()
	bool bIsConstant;

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

