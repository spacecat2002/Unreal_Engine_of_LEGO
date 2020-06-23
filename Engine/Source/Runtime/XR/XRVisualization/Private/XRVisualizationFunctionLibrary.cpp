// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRVisualizationFunctionLibrary.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "XRVisualizationModule.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"


UXRVisualizationLoadHelper::UXRVisualizationLoadHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsDefaultSubobject())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinder<UStaticMesh> GenericHMDMesh;
			ConstructorHelpers::FObjectFinder<UStaticMesh> OculusControllerMesh;
			ConstructorHelpers::FObjectFinder<UStaticMesh> ViveControllerMesh;
			ConstructorHelpers::FObjectFinder<UStaticMesh> STEMControllerMesh;
			FConstructorStatics()
				: GenericHMDMesh(TEXT("/Engine/VREditor/Devices/Generic/GenericHMD.GenericHMD"))
				, OculusControllerMesh(TEXT("/Engine/VREditor/Devices/Oculus/OculusControllerMesh.OculusControllerMesh"))
				, ViveControllerMesh(TEXT("/Engine/VREditor/Devices/Vive/ViveControllerMesh.ViveControllerMesh"))
				, STEMControllerMesh(TEXT("/Engine/VREditor/Devices/STEM/STEMControllerMesh.STEMControllerMesh"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		GenericHMD = ConstructorStatics.GenericHMDMesh.Object;
		OculusControllerMesh = ConstructorStatics.OculusControllerMesh.Object;
		ViveControllerMesh = ConstructorStatics.ViveControllerMesh.Object;
		STEMControllerMesh = ConstructorStatics.STEMControllerMesh.Object;

		if ((GenericHMD == nullptr) || (OculusControllerMesh == nullptr) || (ViveControllerMesh == nullptr) || (STEMControllerMesh == nullptr))
		{
			UE_LOG(LogXRVisual, Error, TEXT("XR Visualizations failed to load"));
		}
	}
}

UXRVisualizationFunctionLibrary::~UXRVisualizationFunctionLibrary()
{
	if (LoadHelper)
	{
		LoadHelper->RemoveFromRoot();
		LoadHelper = nullptr;
	}
}

void UXRVisualizationFunctionLibrary::RenderHMDVisualization(const FXRHMDVisualizationData& XRVisualizationData)
{
	UXRVisualizationFunctionLibrary* Instance = UXRVisualizationFunctionLibrary::StaticClass()->GetDefaultObject<UXRVisualizationFunctionLibrary>();
	check(Instance);
	Instance->VerifyInitMeshes();
	UXRVisualizationLoadHelper* LocalLoadHelper = Instance->LoadHelper;
	check(LocalLoadHelper);

	UStaticMesh* MeshToUse = LocalLoadHelper->GenericHMD;
	FTransform WorldTransform(XRVisualizationData.Rotation, XRVisualizationData.Position);

	FName ActorName(*FString::Printf(TEXT("XR_%s_%x"), *XRVisualizationData.DeviceName.ToString(), (PTRINT)&XRVisualizationData));

	//now render at the proper transform
	if (MeshToUse != nullptr)
	{
		RenderGenericMesh(ActorName, MeshToUse, WorldTransform);
	}
}

void UXRVisualizationFunctionLibrary::RenderMotionControllerVisualization(const FXRMotionControllerVisualizationData& XRVisualizationData, bool bRight)
{
	if (XRVisualizationData.bValid)
	{
		//now render at the proper transform
		if (XRVisualizationData.DeviceVisualType == EXRVisualType::Controller)
		{
			UXRVisualizationFunctionLibrary* Instance = UXRVisualizationFunctionLibrary::StaticClass()->GetDefaultObject<UXRVisualizationFunctionLibrary>();
			check(Instance);
			Instance->VerifyInitMeshes();
			UXRVisualizationLoadHelper* LocalLoadHelper = Instance->LoadHelper;
			check(LocalLoadHelper);

			FTransform WorldTransform(XRVisualizationData.Rotation, XRVisualizationData.Position, bRight ? FVector(1.0f, -1.0f, 1.0f) : FVector(1.0f, 1.0f, 1.0f));

			FName ActorName(*FString::Printf(TEXT("XR_%s_%x"), *XRVisualizationData.DeviceName.ToString(), (PTRINT)&XRVisualizationData));

			UStaticMesh* MeshToUse = nullptr;
			if (XRVisualizationData.DeviceName == TEXT("OculusHMD"))
			{
				MeshToUse = LocalLoadHelper->OculusControllerMesh;
			}
			else if (XRVisualizationData.DeviceName == TEXT("SteamVR"))
			{
				MeshToUse = LocalLoadHelper->ViveControllerMesh;
			}
			else //if (XRVisualizationData.DeviceName == TEXT(""))
			{
				MeshToUse = LocalLoadHelper->STEMControllerMesh;
			}

			if (MeshToUse != nullptr)
			{
				RenderGenericMesh(ActorName, MeshToUse, WorldTransform);
			}
		}
		else
		{
			//add custom hand rendering.
			RenderHandMesh(XRVisualizationData);
		}
	}
}

void UXRVisualizationFunctionLibrary::VerifyInitMeshes()
{
	if (LoadHelper == nullptr)
	{
		LoadHelper = NewObject<UXRVisualizationLoadHelper>(GetTransientPackage(), UXRVisualizationLoadHelper::StaticClass());
		LoadHelper->AddToRoot();
	}
}

void UXRVisualizationFunctionLibrary::RenderGenericMesh(const FName& ActorName, UStaticMesh* Mesh, FTransform& WorldTransform)
{
	ULevel* CurrentLevel = GWorld->GetCurrentLevel();

	AActor* FoundActor = FindObjectFast<AActor>(CurrentLevel, ActorName, true);
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	if ((FoundActor == nullptr) || (FoundActor->IsPendingKill()))
	{
		//create actor
		FoundActor = NewObject<AActor>(CurrentLevel, ActorName);
		//create component
		StaticMeshComponent = NewObject<UStaticMeshComponent>(FoundActor, UStaticMeshComponent::StaticClass());
		StaticMeshComponent->SetStaticMesh(Mesh);
		StaticMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		StaticMeshComponent->RegisterComponentWithWorld(GWorld);
	}
	else
	{
		//get static mesh component out of the actor
		StaticMeshComponent = Cast<UStaticMeshComponent>(FoundActor->GetComponentByClass(UStaticMeshComponent::StaticClass()));
	}

	if (StaticMeshComponent)
	{
		//Update the transform
		StaticMeshComponent->SetWorldTransform(WorldTransform);

		FTimerManager& TimerManager = FoundActor->GetWorldTimerManager();
		//reset this timer and start over
		TimerManager.ClearAllTimersForObject(FoundActor);

		//Add timer to turn the component back off
		FTimerHandle TempHandle;
		TimerManager.SetTimer(TempHandle, FoundActor, &AActor::K2_DestroyActor, 2.0f, true);
	}

}

void UXRVisualizationFunctionLibrary::RenderHandMesh(const FXRMotionControllerVisualizationData& XRVisualizationData)
{
	//if (XRVisualizationData.ApplicationInstanceID == FApp::GetInstanceId())
	//{
	//	//ASK TRACKING SYSTEM FOR SPECIFIC MESH IT MIGHT PROVIDE
	//}
	//else
	{
		RenderFinger(XRVisualizationData, (int32)EHandKeypoint::ThumbMetacarpal, (int32)EHandKeypoint::ThumbTip);
		RenderFinger(XRVisualizationData, (int32)EHandKeypoint::IndexMetacarpal, (int32)EHandKeypoint::IndexTip);
		RenderFinger(XRVisualizationData, (int32)EHandKeypoint::MiddleMetacarpal, (int32)EHandKeypoint::MiddleTip);
		RenderFinger(XRVisualizationData, (int32)EHandKeypoint::RingMetacarpal, (int32)EHandKeypoint::RingTip);
		RenderFinger(XRVisualizationData, (int32)EHandKeypoint::LittleMetacarpal, (int32)EHandKeypoint::LittleTip);
	}
}

void UXRVisualizationFunctionLibrary::RenderFinger(const FXRMotionControllerVisualizationData& XRVisualizationData, int32 FingerStart, int32 FingerEnd)
{
	check((FingerStart > 0) && (FingerStart < EHandKeypointCount));
	check((FingerEnd > 0) && (FingerEnd < EHandKeypointCount));
	if ((EHandKeypointCount == XRVisualizationData.HandKeyPositions.Num()) && (EHandKeypointCount == XRVisualizationData.HandKeyRadii.Num()))
	{
		FColor Color = FColor::Magenta;
		bool bPersistentLines = false;
		float LifeTime = -1.0f;
		uint8 DepthPriority = 0;
		float Thickness = 0.5f;

		//draw 0 - 1 (palm to wrist)
		DrawDebugLine(GWorld, XRVisualizationData.HandKeyPositions[0], XRVisualizationData.HandKeyPositions[1], Color, bPersistentLines, LifeTime, DepthPriority, Thickness);

		//draw 1 - LineStart (wrist to first part of finger)
		DrawDebugLine(GWorld, XRVisualizationData.HandKeyPositions[1], XRVisualizationData.HandKeyPositions[FingerStart], Color, bPersistentLines, LifeTime, DepthPriority, Thickness);

		//Iterate from FingerStart to Finger End
		for (int DigitIndex = FingerStart; DigitIndex < FingerEnd; ++DigitIndex)
		{
			DrawDebugLine(GWorld, XRVisualizationData.HandKeyPositions[DigitIndex], XRVisualizationData.HandKeyPositions[DigitIndex + 1], Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
			FVector Extents(XRVisualizationData.HandKeyRadii[DigitIndex]);
			DrawDebugBox(GWorld, XRVisualizationData.HandKeyPositions[DigitIndex], Extents, FQuat::Identity, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		}
	}
}
