// Copyright Epic Games, Inc. All Rights Reserved.

#include "ODSC.h"
#include "ODSCLog.h"
#include "ODSCManager.h"
#include "Materials/MaterialInterface.h"
#include "ShaderCompiler.h"

#define LOCTEXT_NAMESPACE "FODSCModule"

DEFINE_LOG_CATEGORY(LogODSC);

void FODSCModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	if (nullptr == ODSCManager)
	{
		ODSCManager = new FODSCManager();
	}
}

void FODSCModule::ShutdownModule()
{
	delete ODSCManager;
	ODSCManager = nullptr;
}

bool FODSCModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with ODSC
	if (FParse::Command(&Cmd, TEXT("ODSC")))
	{
		return HandleODSCCommand(Cmd, Ar);
	}
	return false;
}

void FODSCModule::AddThreadedRequest(const TArray<FString>& MaterialsToLoad, EShaderPlatform ShaderPlatform, bool bCompileChangedShaders)
{
	GetODSCManager().AddThreadedRequest(MaterialsToLoad, GMaxRHIShaderPlatform, bCompileChangedShaders);
}

void FODSCModule::AddThreadedShaderPipelineRequest(EShaderPlatform ShaderPlatform, const FString& MaterialName, const FString& VertexFactoryName, const FString& PipelineName, const TArray<FString>& ShaderTypeNames)
{
	GetODSCManager().AddThreadedShaderPipelineRequest(ShaderPlatform, MaterialName, VertexFactoryName, PipelineName, ShaderTypeNames);
}

bool FODSCModule::HandleODSCCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	return true;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FODSCModule, ODSC)
