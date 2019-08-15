// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RigVMPin.h"
#include "RigVMLink.generated.h"

class URigVMGraph;

UCLASS(BlueprintType)
class RIGVM_API URigVMLink : public UObject
{
	GENERATED_BODY()

public:

	URigVMLink()
	{
		SourcePin = TargetPin = nullptr;
	}

	virtual void Serialize(FArchive& Ar) override;

	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	int32 GetLinkIndex() const;

	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMGraph* GetGraph() const;

	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMPin* GetSourcePin();

	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMPin* GetTargetPin();

	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	FString GetPinPathRepresentation();

private:

	FString SourcePinPath;
	FString TargetPinPath;

	URigVMPin* SourcePin;
	URigVMPin* TargetPin;

	friend class URigVMController;
};

