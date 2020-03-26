// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMFunction.h"

/**
 * The Prototype argument represents a single parameter
 * in a function call and all of its possible types
 */
struct RIGVM_API FRigVMPrototypeArg
{
	// returns the name of the argument
	const FName& GetName() const { return Name; }

	// returns true if this argument supports a given type
	bool SupportsType(const FString& InCPPType) const;

	// returns an array of all of the supported types
	TArray<FString> GetSupportedTypes() const;

	// returns true if an argument is singleton (same type for all variants)
	bool IsSingleton() const;

protected:

	bool SupportsType(const FString& InCPPType, TArray<int32> InFunctionIndices) const;

	bool IsSingleton(TArray<int32> InFunctionIndices) const;

	FName Name;
	bool bSingleton;
	TArray<FString> Types;

	// default constructor
	FRigVMPrototypeArg();

	// constructor from a property
	FRigVMPrototypeArg(const FProperty* InProperty);

	friend struct FRigVMPrototype;
};

/**
 * The Prototype is used to group multiple rigvm functions
 * that share the same notation. Prototypes can then be used
 * to build polymorphic nodes (RigVMPrototypeNode) that can
 * take on any of the types supported by the prototype.
 */
struct RIGVM_API FRigVMPrototype
{
public:

	typedef TMap<FName, FString> FTypeMap;

	// returns true if this is a valid prototype
	bool IsValid() const;

	// Returns the notation of this prototype
	const FName& GetNotation() const;

	// returns true if this prototype is compatible with another one
	bool IsCompatible(const FRigVMPrototype& InOther) const;

	// returns true if this prototype can merge another one
	bool Merge(const FRigVMPrototype& InOther);

	// returns the number of args of this prototype
	int32 NumArgs() const { return Args.Num(); }

	// returns an argument for a given index
	const FRigVMPrototypeArg* GetArg(int32 InIndex) const { return &Args[InIndex]; }

		// returns an argument given a name (or nullptr)
	const FRigVMPrototypeArg* FindArg(const FName& InArgName) const;

	// returns true if a given arg supports a type
	bool ArgSupportsType(const FName& InArgName, const FString& InCPPType, const FTypeMap& InTypes = FTypeMap()) const;

	// returns the number of functions supported by this prototype
	int32 NumFunctions() const { return Functions.Num(); }

	// returns a function given an index
	const FRigVMFunction* GetFunction(int32 InIndex) const;

	// returns true if the prototype was able to resolve
	bool Resolve(FTypeMap& InOutTypes, int32& OutFunctionIndex) const;

	static FName GetNotationFromStruct(UScriptStruct* InStruct, const FString& InPrototypeName);

private:

	// Default constructor
	FRigVMPrototype();

	// Constructor from a struct and a function name
	FRigVMPrototype(UScriptStruct* InStruct, const FString& InPrototypeName, int32 InFunctionIndex);

	int32 Index;
	FName Notation;
	TArray<FRigVMPrototypeArg> Args;
	TArray<int32> Functions;

	friend struct FRigVMRegistry;
};

