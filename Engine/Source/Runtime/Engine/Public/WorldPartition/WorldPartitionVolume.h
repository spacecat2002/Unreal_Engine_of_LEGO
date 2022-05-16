// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartitionVolume.generated.h"

class FLoaderAdapterActor;

/**
 * A world partition volume to allow loading cells inside (editor-only)
 */
UCLASS()
class ENGINE_API AWorldPartitionVolume : public AVolume, public IWorldPartitionActorLoaderInterface
{
	GENERATED_BODY()

public:
	AWorldPartitionVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	//~ Begin AActor Interface
	virtual bool IsEditorOnly() const override { return true; }
#if WITH_EDITOR
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
	virtual bool SupportsDataLayer() const override { return false; }
#endif
	//~ End AActor Interface

#if WITH_EDITOR
	//~ Begin IWorldPartitionActorLoaderInterface interface
	virtual ILoaderAdapter* GetLoaderAdapter() override;
	//~ End IWorldPartitionActorLoaderInterface interface
#endif

#if WITH_EDITOR
private:
	FLoaderAdapterActor* WorldPartitionActorLoader;
#endif
};