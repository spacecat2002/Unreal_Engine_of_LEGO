// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMTraits.h"

typedef TArray<uint8> FRigVMByteArray;
typedef TArray<FRigVMByteArray> FRigVMNestedByteArray;

/**
 * The FRigVMDynamicArray is used as an array wrapping a generic TArray.
 * FRigVMDynamicArray is copied by reference.
 */
template<class T>
struct RIGVM_API FRigVMDynamicArray
{
public:

	typedef       T* RangedForIteratorType;
	typedef const T* RangedForConstIteratorType;

	FORCEINLINE FRigVMDynamicArray(FRigVMByteArray& InStorage)
	: Storage(InStorage)
	{
	}

	// copy constructor
	FORCEINLINE FRigVMDynamicArray(const FRigVMDynamicArray& InOther)
	: Storage(InOther.Storage)
	{
	}

	// assignment operator
	FORCEINLINE FRigVMDynamicArray& operator= (const FRigVMDynamicArray &InOther)
	{
		Storage = InOther.Storage;
		return *this;
	}

	// returns the number of elements in this array
	FORCEINLINE int32 Num() const
	{
		return Storage.Num() / sizeof(T);
	}

	// returns true if a given Index is valid
	FORCEINLINE bool IsValidIndex(int32 InIndex) const
	{
		return Storage.IsValidIndex(InIndex * sizeof(T));
	}

	// empties the contents of the array
	FORCEINLINE void Reset()
	{
		if (Num() > 0)
		{
			RigVMDestroy<T>(GetData(), Num());
		}
		Storage.Reset();
	}

	// adds an element to the array
	FORCEINLINE int32 Add(const T& InValue)
	{
		int32 ElementIndex = Num();
		int32 ByteIndex = Storage.Num();
		Storage.SetNum(ByteIndex + sizeof(T));
		T* Data = (T*)&Storage[ByteIndex];
		*Data = InValue;
		return ElementIndex;
	}

	// appends an array to this storage and returns the first index.
	FORCEINLINE int32 Append(const FRigVMDynamicArray& InOther)
	{
		if(InOther.Num() == 0)
		{
			return INDEX_NONE;
		}

		int32 FirstIndex = Num();
		for(const T& Element : InOther)
		{
			Add(Element);
		}
		return FirstIndex;
	}

	// appends an array to this storage and returns the first index.
	FORCEINLINE int32 Append(const TArray<T>& InOther)
	{
		if(InOther.Num() == 0)
		{
			return INDEX_NONE;
		}

		int32 FirstIndex = Num();
		for(const T& Element : InOther)
		{
			Add(Element);
		}
		return FirstIndex;
	}

	// sets the number of elements in this array
	FORCEINLINE void SetNumUninitialized(int32 InSize)
	{
		if (InSize == 0)
		{
			Reset();
			return;
		}

		int32 LastSize = Num();
		if (LastSize > InSize)
		{
			RigVMDestroy<T>(GetData() + LastSize, InSize - LastSize);
		}
		Storage.SetNumUninitialized(InSize * sizeof(T), false /* no shrinking */);
	}

	// sets the number of elements in this array
	FORCEINLINE void SetNumZeroed(int32 InSize)
	{
		int32 LastSize = Num();
		SetNumUninitialized(InSize);

		if (LastSize < InSize)
		{
			FMemory::Memzero(GetData() + LastSize, InSize - LastSize);
		}
	}

	// sets the number of elements in this array
	FORCEINLINE void SetNum(int32 InSize)
	{
		int32 LastSize = Num();
		SetNumUninitialized(InSize);

		if (LastSize < InSize)
		{
			RigVMInitialize<T>(GetData() + LastSize, InSize - LastSize);
		}
	}

	FORCEINLINE const T& operator[](int32 InIndex) const
	{
		return *(const T*)&Storage[InIndex * sizeof(T)];
	}

	FORCEINLINE T& operator[](int32 InIndex)
	{
		return *(T*)&Storage[InIndex * sizeof(T)];
	}

	FORCEINLINE bool operator ==(const FRigVMDynamicArray& Other) const
	{
		return Storage == Other.Storage;
	}
	
	FORCEINLINE bool operator !=(const FRigVMDynamicArray& Other) const
	{
		return Storage != Other.Storage;
	}

	FORCEINLINE const T* GetData() const
	{
		return (const T*)Storage.GetData();
	}

	FORCEINLINE T* GetData()
	{
		return (T*)Storage.GetData();
	}

	FORCEINLINE RangedForIteratorType      begin()       { return GetData(); }
	FORCEINLINE RangedForConstIteratorType begin() const { return GetData(); }
	FORCEINLINE RangedForIteratorType      end()         { return GetData() + Num(); }
	FORCEINLINE RangedForConstIteratorType end() const   { return GetData() + Num(); }

	FORCEINLINE operator TArray<T>() const
	{
		TArray<T> Result;
		Result.Reserve(Num());
		for(const T& Element : *this)
		{
			Result.Add(Element);
		}
		return Result;
	}

private:

	FRigVMByteArray& Storage;
};

/**
 * The FRigVMFixedArray is used as an alternative to TArrayView
 * FRigVMFixedArray is copied by reference.
 */
template<class T>
struct RIGVM_API FRigVMFixedArray
{
public:

	typedef       T* RangedForIteratorType;
	typedef const T* RangedForConstIteratorType;

	// default constructor
	FORCEINLINE FRigVMFixedArray()
	: Data(nullptr)
	, Size(0)
	{
	}

	// constructor from a typed TArray
	FORCEINLINE FRigVMFixedArray(const TArray<T>& InStorage)
	: Data((T*)InStorage.GetData())
	, Size(InStorage.Num())
	{
	}

	// constructor from direct memory
	FORCEINLINE FRigVMFixedArray(T* InData, int32 InSize)
	: Data(InData)
	, Size(InSize)
	{
	}

	// constructor from an dynamic array
	FORCEINLINE FRigVMFixedArray(FRigVMDynamicArray<T> InDynamicArray)
	: Data(InDynamicArray.GetData())
	, Size(InDynamicArray.Num())
	{
	}

	// copy constructor
	FORCEINLINE FRigVMFixedArray(const FRigVMFixedArray& InOther)
	: Data(InOther.Data)
	, Size(InOther.Size)
	{
	}

	// assignment operator
	FORCEINLINE FRigVMFixedArray& operator= (const FRigVMFixedArray &InOther)
	{
		Data = InOther.Data;
		Size = InOther.Size;
		return *this;
	}

	// returns the number of elements in this array
	FORCEINLINE int32 Num() const
	{
		return Size;
	}

	// returns true if a given Index is valid
	FORCEINLINE bool IsValidIndex(int32 InIndex) const
	{
		return InIndex > INDEX_NONE && InIndex < Size;
	}

	FORCEINLINE const T& operator[](int32 InIndex) const
	{
		return Data[InIndex];
	}

	FORCEINLINE T& operator[](int32 InIndex)
	{
		return Data[InIndex];
	}

	FORCEINLINE bool operator ==(const FRigVMDynamicArray<T>& Other) const
	{
		return Data == Other.Data;
	}
	
	FORCEINLINE bool operator !=(const FRigVMDynamicArray<T>& Other) const
	{
		return Data != Other.Data;
	}

	FORCEINLINE const T* GetData() const
	{
		return Data;
	}

	FORCEINLINE T* GetData()
	{
		return Data;
	}

	FORCEINLINE const FRigVMFixedArray Slice(int32 StartIndex, int32 Count) const
	{
		return FRigVMFixedArray((T*)GetData() + StartIndex, Count);
	}

	FORCEINLINE FRigVMFixedArray Slice(int32 StartIndex, int32 Count)
	{
		return FRigVMFixedArray(GetData() + StartIndex, Count);
	}

	FORCEINLINE RangedForIteratorType      begin()       { return GetData(); }
	FORCEINLINE RangedForConstIteratorType begin() const { return GetData(); }
	FORCEINLINE RangedForIteratorType      end()         { return GetData() + Num(); }
	FORCEINLINE RangedForConstIteratorType end() const   { return GetData() + Num(); }

	FORCEINLINE operator TArray<T>() const
	{
		TArray<T> Result;
		Result.Reserve(Num());
		for(const T& Element : *this)
		{
			Result.Add(Element);
		}
		return Result;
	}

private:

	T* Data;
	int32 Size;
};

