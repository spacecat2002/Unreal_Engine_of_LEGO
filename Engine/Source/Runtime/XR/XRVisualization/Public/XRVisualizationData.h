// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "XRVisualizationData.generated.h"

/**
 * Transforms that are tracked on the hand.
 * Matches the enums from WMR to make it a direct mapping
 */
UENUM(BlueprintType)
enum class EHandKeypoint : uint8
{
	Palm,
	Wrist,
	ThumbMetacarpal,
	ThumbProximal,
	ThumbDistal,
	ThumbTip,
	IndexMetacarpal,
	IndexProximal,
	IndexIntermediate,
	IndexDistal,
	IndexTip,
	MiddleMetacarpal,
	MiddleProximal,
	MiddleIntermediate,
	MiddleDistal,
	MiddleTip,
	RingMetacarpal,
	RingProximal,
	RingIntermediate,
	RingDistal,
	RingTip,
	LittleMetacarpal,
	LittleProximal,
	LittleIntermediate,
	LittleDistal,
	LittleTip
};

const int32 EHandKeypointCount = static_cast<int32>(EHandKeypoint::LittleTip) + 1;

UENUM(BlueprintType)
enum class EXRVisualType : uint8
{
	Controller,
	Hand
};

USTRUCT(BlueprintType)
struct XRVISUALIZATION_API FXRHMDVisualizationData
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadOnly, Category = "XR")
	bool bValid;
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	FName DeviceName;
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	FGuid ApplicationInstanceID;

	UPROPERTY(BlueprintReadOnly, Category = "XR")
	FVector Position;
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	FQuat Rotation;
};

USTRUCT(BlueprintType)
struct XRVISUALIZATION_API FXRMotionControllerVisualizationData
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadOnly, Category = "XR")
	bool bValid;
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	FName DeviceName;
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	FGuid ApplicationInstanceID;
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	EXRVisualType DeviceVisualType;
	
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	FVector Position;
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	FQuat Rotation;

	UPROPERTY(BlueprintReadOnly, Category = "XR")
	TArray<struct FVector> HandKeyPositions;
	UPROPERTY(BlueprintReadOnly, Category = "XR")
	TArray<float> HandKeyRadii;
};
