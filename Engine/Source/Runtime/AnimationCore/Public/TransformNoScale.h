// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TransformNoScale.generated.h"

USTRUCT(BlueprintType)
struct ANIMATIONCORE_API FTransformNoScale
{
	GENERATED_BODY()

	/**
	 * The identity transformation (Rotation = FRotator::ZeroRotator, Translation = FVector::ZeroVector, Scale = (1,1,1)).
	 */
	static const FTransformNoScale Identity;

	FORCEINLINE FTransformNoScale()
		: Location(ForceInitToZero)
		, Rotation(ForceInitToZero)
	{
	}

	FORCEINLINE FTransformNoScale(const FVector& InLocation, const FQuat& InRotation)
		: Location(InLocation)
		, Rotation(InRotation)
	{
	}

	FORCEINLINE FTransformNoScale(const FTransform& InTransform)
		: Location(InTransform.GetLocation())
		, Rotation(InTransform.GetRotation())
	{
	}

	FORCEINLINE FTransformNoScale& operator =(const FTransform& InTransform)
	{
		FromFTransform(InTransform);
		return *this;
	}

	FORCEINLINE operator FTransform() const
	{
		return ToFTransform();
	}

	/** The translation of this transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	FVector Location;

	/** The rotation of this transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	FQuat Rotation;

	/** Convert to an FTransform */
	FORCEINLINE FTransform ToFTransform() const
	{
		return FTransform(Rotation, Location, FVector::OneVector);
	}

	/** Convert from an FTransform */
	FORCEINLINE void FromFTransform(const FTransform& InTransform)
	{
		Location = InTransform.GetLocation();
		Rotation = InTransform.GetRotation();
	}
};