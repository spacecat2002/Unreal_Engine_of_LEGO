// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMArray.h"
#include "RigVMExecuteContext.generated.h"

USTRUCT()
struct RIGVM_API FRigVMSlice
{
public:

	GENERATED_BODY()

	FORCEINLINE FRigVMSlice()
	: LowerBound(0)
	, UpperBound(0)
	, Index(INDEX_NONE)
	{
		Reset();
	}

	FORCEINLINE FRigVMSlice(int32 InCount)
		: LowerBound(0)
		, UpperBound(InCount - 1)
		, Index(INDEX_NONE)
	{
		Reset();
	}

	FORCEINLINE FRigVMSlice(int32 InCount, const FRigVMSlice& InParent)
		: LowerBound(InParent.GetIndex() * InCount)
		, UpperBound((InParent.GetIndex() + 1) * InCount - 1)
		, Index(INDEX_NONE)
	{
		Reset();
	}

	FORCEINLINE bool IsValid() const
	{ 
		return Index != INDEX_NONE;
	}
	
	FORCEINLINE bool IsComplete() const
	{
		return Index > UpperBound;
	}

	FORCEINLINE int32 GetIndex() const
	{
		return Index;
	}

	FORCEINLINE int32 Num() const
	{
		return 1 + UpperBound - LowerBound;
	}

	FORCEINLINE int32 TotalNum() const
	{
		return UpperBound + 1;
	}

	FORCEINLINE operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator !() const
	{
		return !IsValid();
	}

	FORCEINLINE operator int32() const
	{
		return Index;
	}

	FORCEINLINE FRigVMSlice& operator++()
	{
		Index++;
		return *this;
	}

	FORCEINLINE FRigVMSlice operator++(int32)
	{
		FRigVMSlice TemporaryCopy = *this;
		++*this;
		return TemporaryCopy;
	}

	FORCEINLINE bool Next()
	{
		if (!IsValid())
		{
			return false;
		}

		if (IsComplete())
		{
			return false;
		}

		Index++;
		return true;
	}

	FORCEINLINE void Reset()
	{
		if (UpperBound >= LowerBound)
		{
			Index = LowerBound;
		}
		else
		{
			Index = INDEX_NONE;
		}
	}

private:

	int32 LowerBound;
	int32 UpperBound;
	int32 Index;
};

/**
 * The execute context is used for mutable nodes to
 * indicate execution order.
 */
USTRUCT()
struct RIGVM_API FRigVMExecuteContext
{
	GENERATED_BODY()

	FORCEINLINE FRigVMExecuteContext()
		: EventName(NAME_None)
		, FunctionName(NAME_None)
		, InstructionIndex(0)
	{
		Reset();
	}

	FORCEINLINE void Reset()
	{
		Slices.Reset();
		Slices.Add(FRigVMSlice());
		InstructionIndex = 0;
	}

	FORCEINLINE const FRigVMSlice& GetSlice() const
	{
		return Slices.Last();
	}

	FORCEINLINE void BeginSlice(int32 InCount)
	{
		ensure(!IsSliceComplete());
		Slices.Add(FRigVMSlice(InCount, Slices.Last()));
	}

	FORCEINLINE void EndSlice()
	{
		ensure(Slices.Num() > 1);
		Slices.Pop();
	}

	FORCEINLINE void IncrementSlice()
	{
		FRigVMSlice& ActiveSlice = Slices.Last();
		ActiveSlice++;
	}

	FORCEINLINE bool IsSliceComplete() const
	{
		return GetSlice().IsComplete();
	}

	FName EventName;
	FName FunctionName;
	uint16 InstructionIndex;
	FRigVMFixedArray<void*> OpaqueArguments;
	TArray<FRigVMSlice> Slices;
};
