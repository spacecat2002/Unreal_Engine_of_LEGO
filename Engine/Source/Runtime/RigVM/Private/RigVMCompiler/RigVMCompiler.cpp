// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMModel/RigVMGraphUtils.h"
#include "RigVMModule.h"

FRigVMCompileSettings::FRigVMCompileSettings()
	: SurpressInfoMessage(true)
	, SurpressWarnings(false)
	, SurpressErrors(false)
{
}

URigVMCompiler::URigVMCompiler()
{
}

bool URigVMCompiler::Compile(URigVMGraph* InGraph, URigVM* OutVM)
{
	if (InGraph == nullptr)
	{
		ReportError(TEXT("Provided graph is nullptr."));
		return false;
	}
	if (OutVM == nullptr)
	{
		ReportError(TEXT("Provided vm is nullptr."));
		return false;
	}

	// a) get the generic execution order for the nodes
	// b) figure out blocks and number of executions for blocks
	// c) register all functions to VM
	// d) find all literals based on defaults and define them (for now flatten them - no sharing)
	// e) define all parameters
	// f) define all variables
	// g) define all work state (for now no sharing)
	// h) build out the byte code the the required instructions
	// i) fix up the jump instructions for all blocks

	return true;
}

void URigVMCompiler::ReportInfo(const FString& InMessage)
{
	if (!Settings.SurpressInfoMessage)
	{
		UE_LOG(LogRigVM, Display, TEXT("%s"), *InMessage);
	}
}

void URigVMCompiler::ReportWarning(const FString& InMessage)
{
	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Warning, *InMessage, *FString());
}

void URigVMCompiler::ReportError(const FString& InMessage)
{
	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, *InMessage, *FString());
}
