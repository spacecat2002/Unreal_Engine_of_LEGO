// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RigVMPin.generated.h"

class URigVMGraph;
class URigVMNode;

UENUM()
enum class ERigVMPinDirection : uint8
{
	Input,
	Output,
	IO,
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
	bool IsArray() const;
	bool IsArrayElement() const;
	int32 GetIndex() const;
	FString GetCPPType() const;

	URigVMPin* GetParentPin() const;
	const TArray<URigVMPin*>& GetSubPins() const;
	URigVMPin* FindSubPin(const FString& InPinPath) const;
	const TArray<URigVMPin*>& GetConnectedPins() const;

	URigVMNode* GetNode() const;
	URigVMGraph* GetGraph() const;

private:

	UPROPERTY()
	ERigVMPinDirection Direction;

	UPROPERTY()
	bool bIsConstant;

	UPROPERTY()
	int32 Index;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	TArray<URigVMPin*> SubPins;

	UPROPERTY()
	TArray<URigVMPin*> ConnectedPins;

	friend class URigVMController;
};

