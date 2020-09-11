// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleManager.h"

class FODSCManager;

class FODSCModule : public IModuleInterface, public FSelfRegisteringExec
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// FSelfRegisteringExec

	/**
	 * Handle exec commands starting with "ODSC"
	 *
	 * @param InWorld	the world context
	 * @param Cmd		the exec command being executed
	 * @param Ar		the archive to log results to
	 *
	 * @return true if the handler consumed the input, false to continue searching handlers
	 */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Add a request to compile shaders.  The results are submitted and processed in an async manner.
	 *
	 * @param MaterialsToCompile - List of material names to submit compiles for.
	 * @param ShaderPlatform - Which shader platform to compile for.
	 * @param bCompileChangedShaders - Whether or not we should recompile shaders that have changed.
	 *
	 * @return false if no longer needs ticking
	 */
	void AddThreadedRequest(const TArray<FString>& MaterialsToLoad, EShaderPlatform ShaderPlatform, bool bCompileChangedShaders);

	/**
	 * Add a request to compile a pipeline (VS/PS) of shaders.  The results are submitted and processed in an async manner.
	 *
	 * @param ShaderPlatform - Which shader platform to compile for.
	 * @param MaterialName - The name of the material to compile.
	 * @param VertexFactoryName - The name of the vertex factory type we should compile.
	 * @param PipelineName - The name of the shader pipeline we should compile.
	 * @param ShaderTypeNames - The shader type names of all the shader stages in the pipeline.
	 *
	 * @return false if no longer needs ticking
	 */
	void AddThreadedShaderPipelineRequest(EShaderPlatform ShaderPlatform, const FString& MaterialName, const FString& VertexFactoryName, const FString& PipelineName, const TArray<FString>& ShaderTypeNames);

private:

	/**
	 * Handler for Exec commands
	 */
	bool HandleODSCCommand(const TCHAR* Cmd, FOutputDevice& Ar);

	inline FODSCManager& GetODSCManager()
	{
		check(ODSCManager != nullptr);
		return *ODSCManager;
	}

	/** Keeps track of shader compile requests while they are being processed by the cook on the fly server */
	FODSCManager* ODSCManager = nullptr;
};
