// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMMemory.h"
#include "RigVMRegistry.h"
#include "RigVMByteCode.h"
#include "RigVM.generated.h"

// The type of parameter for a VM
UENUM(BlueprintType)
enum class ERigVMParameterType : uint8
{
	Input,
	Output,
	Invalid
};

/**
 * The RigVMParameter define an input or output of the RigVM.
 * Parameters are mapped to work state memory registers and can be
 * used to set input parameters as well as retrieve output parameters.
 */
USTRUCT(BlueprintType)
struct FRigVMParameter
{
	GENERATED_BODY()

public:

	FRigVMParameter()
		: Type(ERigVMParameterType::Invalid)
		, Name(NAME_None)
		, RegisterIndex(INDEX_NONE)
	{
	}

	// returns the type of this parameter
	ERigVMParameterType GetType() const { return Type; }

	// returns the name of this parameters
	const FName& GetName() const { return Name; }

	// returns the register index of this parameter in the work memory
	int32 GetRegisterIndex() const { return RegisterIndex; }

private:

	FRigVMParameter(ERigVMParameterType InType, const FName& InName, int32 InRegisterIndex)
		: Type(InType)
		, Name(InName)
		, RegisterIndex(InRegisterIndex)
	{
	}

	UPROPERTY(BlueprintReadonly, Category = RigVMParameter, meta = (AllowPrivateAccess = "true"))
	ERigVMParameterType Type;

	UPROPERTY(BlueprintReadonly, Category = RigVMParameter, meta = (AllowPrivateAccess = "true"))
	FName Name;

	UPROPERTY(BlueprintReadonly, Category = RigVMParameter, meta = (AllowPrivateAccess = "true"))
	int32 RegisterIndex;

	friend class URigVM;
};

/**
 * The RigVM is the main object for evaluating FRigVMByteCode instructions.
 * It combines the byte code, a list of required function pointers for 
 * execute instructions and required memory in one class.
 */
UCLASS(BlueprintType)
class RIGVM_API URigVM : public UObject
{
	GENERATED_BODY()

public:

	URigVM();
	virtual ~URigVM();

	// resets the container and removes all memory
	void Reset();

	// Executes the VM.
	// You can optionally provide external memory to the execution
	// and provide optional additional operands.
	bool Execute(FRigVMMemoryContainerPtrArray Memory, TArrayView<void*> AdditionalArguments);

	// Executes the VM.
	// You can optionally provide external memory to the execution
	// and provide optional additional operands.
	UFUNCTION(BlueprintCallable, Category = RigVM)
	bool Execute();

	// Add a function for execute instructions to this VM.
	// Execute instructions can then refer to the function by index.
	UFUNCTION()
	int32 AddRigVMFunction(UScriptStruct* InRigVMStruct, const FName& InMethodName);

	// The default mutable work memory
	UPROPERTY()
	FRigVMMemoryContainer WorkMemory;

	// The default const literal memory
	UPROPERTY()
	FRigVMMemoryContainer LiteralMemory;

	// The byte code of the VM
	UPROPERTY()
	FRigVMByteCode ByteCode;

	// Returns the instructions of the VM
	const FRigVMInstructionArray& GetInstructions();

	// Adds a new input / output to the VM
	template<class T>
	FORCEINLINE FRigVMParameter AddPlainParameter(ERigVMParameterType InParameterType, const FName& InName, T DefaultValue)
	{
		ensure(InParameterType != ERigVMParameterType::Invalid);
		int32 RegisterIndex = WorkMemory.AddPlain<T>(WorkMemory.SupportsNames() ? InName : NAME_None, DefaultValue);
		if (RegisterIndex == INDEX_NONE)
		{
			return FRigVMParameter();
		}
		FName Name = WorkMemory.SupportsNames() ? WorkMemory[RegisterIndex].Name : InName;
		FRigVMParameter Parameter(InParameterType, Name, RegisterIndex);
		ParametersNameMap.Add(Parameter.Name, Parameters.Add(Parameter));
		return Parameter;
	}

	// Adds a new input / output to the VM
	template<class T>
	FORCEINLINE FRigVMParameter AddStructParameter(ERigVMParameterType InParameterType, const FName& InName, T DefaultValue)
	{
		ensure(InParameterType != ERigVMParameterType::Invalid);
		int32 RegisterIndex = WorkMemory.AddStruct<T>(WorkMemory.SupportsNames() ? InName : NAME_None, DefaultValue);
		if (RegisterIndex == INDEX_NONE)
		{
			return FRigVMParameter();
		}
		FName Name = WorkMemory.SupportsNames() ? WorkMemory[RegisterIndex].Name : InName;
		FRigVMParameter Parameter(InParameterType, Name, RegisterIndex);
		ParametersNameMap.Add(Parameter.Name, Parameters.Add(Parameter));
		return Parameter;
	}

	// Retrieve the value of a parameter
	template<class T>
	const T& GetParameterValue(const FRigVMParameter& InParameter) const
	{
		return WorkMemory.GetRef<T>(InParameter.GetRegisterIndex());
	}

	// Retrieve the value of a parameter given its index
	template<class T>
	const T& GetParameterValue(int32 InParameterIndex) const
	{
		return GetParameterValue<T>(Parameters[InParameterIndex]);
	}

	// Retrieve the value of a parameter given its name
	template<class T>
	const T& GetParameterValue(const FName& InParameterName) const
	{
		int32 ParameterIndex = ParametersNameMap.FindChecked(InParameterName);
		return GetParameterValue<T>(ParameterIndex);
	}

	// Set the value of a parameter
	template<class T>
	void SetParameterValue(const FRigVMParameter& InParameter, const T& InNewValue)
	{
		WorkMemory.GetRef<T>(InParameter.GetRegisterIndex()) = InNewValue;
	}

	// Set the value of a parameter given its index
	template<class T>
	void SetParameterValue(int32 ParameterIndex, const T& InNewValue)
	{
		return SetParameterValue<T>(Parameters[ParameterIndex], InNewValue);
	}

	// Set the value of a parameter given its name
	template<class T>
	void SetParameterValue(const FName& InParameterName, const T& InNewValue)
	{
		int32 ParameterIndex = ParametersNameMap.FindChecked(InParameterName);
		return SetParameterValue<T>(ParameterIndex, InNewValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	bool GetParameterValueBool(const FName& InParameterName) const
	{
		return GetParameterValue<bool>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	float GetParameterValueFloat(const FName& InParameterName) const
	{
		return GetParameterValue<float>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	int32 GetParameterValueInt(const FName& InParameterName) const
	{
		return GetParameterValue<int32>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FName GetParameterValueName(const FName& InParameterName) const
	{
		return GetParameterValue<FName>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FString GetParameterValueString(const FName& InParameterName) const
	{
		return GetParameterValue<FString>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FVector2D GetParameterValueVector2D(const FName& InParameterName) const
	{
		return GetParameterValue<FVector2D>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FVector GetParameterValueVector(const FName& InParameterName) const
	{
		return GetParameterValue<FVector>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FQuat GetParameterValueQuat(const FName& InParameterName) const
	{
		return GetParameterValue<FQuat>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	FTransform GetParameterValueTransform(const FName& InParameterName) const
	{
		return GetParameterValue<FTransform>(InParameterName);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueBool(const FName& InParameterName, bool InValue)
	{
		SetParameterValue<bool>(InParameterName, InValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueFloat(const FName& InParameterName, float InValue)
	{
		SetParameterValue<float>(InParameterName, InValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueInt(const FName& InParameterName, int32 InValue)
	{
		SetParameterValue<int32>(InParameterName, InValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueName(const FName& InParameterName, const FName& InValue)
	{
		SetParameterValue<FName>(InParameterName, InValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueString(const FName& InParameterName, const FString& InValue)
	{
		SetParameterValue<FString>(InParameterName, InValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueVector2D(const FName& InParameterName, const FVector2D& InValue)
	{
		SetParameterValue<FVector2D>(InParameterName, InValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueVector(const FName& InParameterName, const FVector& InValue)
	{
		SetParameterValue<FVector>(InParameterName, InValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueQuat(const FName& InParameterName, const FQuat& InValue)
	{
		SetParameterValue<FQuat>(InParameterName, InValue);
	}

	UFUNCTION(BlueprintCallable, Category = RigVM)
	void SetParameterValueTransform(const FName& InParameterName, const FTransform& InValue)
	{
		SetParameterValue<FTransform>(InParameterName, InValue);
	}

private:

	void ResolveFunctionsIfRequired();
	void RefreshInstructionsIfRequired();

	UPROPERTY(transient)
	FRigVMInstructionArray Instructions;

	UPROPERTY()
	TArray<FString> FunctionNames;

	TArray<FRigVMFunctionPtr> Functions;

	UPROPERTY()
	TArray<FRigVMParameter> Parameters;

	UPROPERTY()
	TMap<FName, int32> ParametersNameMap;
};
