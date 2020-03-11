// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMStruct.generated.h"

/**
 * The base class for all RigVM enabled structs.
 */
USTRUCT()
struct RIGVM_API FRigVMStruct
{
	GENERATED_BODY()

	virtual ~FRigVMStruct() {}
	virtual FString ProcessPinLabelForInjection(const FString& InLabel) const { return InLabel; }
	virtual FName DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const { return NAME_None; }

public:

	FORCEINLINE virtual int32 GetArraySize(const FName& InParameterName, const FRigVMUserDataArray& RigVMUserData) { return INDEX_NONE; }

	static const FName DeprecatedMetaName;
	static const FName InputMetaName;
	static const FName OutputMetaName;
	static const FName AbstractMetaName;
	static const FName CategoryMetaName;
	static const FName DisplayNameMetaName;
	static const FName MenuDescSuffixMetaName;
	static const FName ShowVariableNameInTitleMetaName;
	static const FName CustomWidgetMetaName;
	static const FName ConstantMetaName;
	static const FName TitleColorMetaName;
	static const FName NodeColorMetaName;
	static const FName KeywordsMetaName;
	static const FName PrototypeNameMetaName;
	static const FName ExpandPinByDefaultMetaName;
	static const FName DefaultArraySizeMetaName;
	static const FName VaryingMetaName;
};
