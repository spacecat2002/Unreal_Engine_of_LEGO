// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "XRVisualizationData.h"
#include "XRVisualizationFunctionLibrary.generated.h"

UCLASS()
class UXRVisualizationLoadHelper : public UObject
{
	GENERATED_BODY()

	/** UObject constructor that takes an optional ObjectInitializer */
	UXRVisualizationLoadHelper(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	UPROPERTY()
	class UStaticMesh* GenericHMD;
	UPROPERTY()
	class UStaticMesh* OculusControllerMesh;
	UPROPERTY()
	class UStaticMesh* ViveControllerMesh;
	UPROPERTY()
	class UStaticMesh* STEMControllerMesh;
};

UCLASS()
class XRVISUALIZATION_API UXRVisualizationFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	virtual ~UXRVisualizationFunctionLibrary();

	/**
	 * Renders HMD Visualization on a system that might not have that HMD as native
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static void RenderHMDVisualization(const FXRHMDVisualizationData& XRVisualizationData);

	/**
	 * Renders Motion Controller Visualization on a system that might not have that HMD as native
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static void RenderMotionControllerVisualization(const FXRMotionControllerVisualizationData& XRVisualizationData, bool bRight);

private:
	void VerifyInitMeshes();

	static void RenderGenericMesh(const FName& ActorName, UStaticMesh* Mesh, FTransform& WorldTransform);
	static void RenderHandMesh(const FXRMotionControllerVisualizationData& XRVisualizationData);
	static void RenderFinger(const FXRMotionControllerVisualizationData& XRVisualizationData, int32 FingerStart, int32 FingerEnd);

	UPROPERTY()
	UXRVisualizationLoadHelper* LoadHelper;
};
