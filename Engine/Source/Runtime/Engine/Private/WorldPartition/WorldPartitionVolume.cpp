// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionVolume.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterActor.h"
#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "WorldPartitionEditor"

AWorldPartitionVolume::AWorldPartitionVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

#if WITH_EDITOR
	if (!IsTemplate() && GetWorld() && GetWorld()->GetWorldPartition())
	{
		WorldPartitionActorLoader = new FLoaderAdapterActor(this);
	}
#endif
}

void AWorldPartitionVolume::BeginDestroy()
{
#if WITH_EDITOR
	if (WorldPartitionActorLoader)
	{
		delete WorldPartitionActorLoader;
		WorldPartitionActorLoader = nullptr;
	}
#endif

	Super::BeginDestroy();
}

#if WITH_EDITOR
IWorldPartitionActorLoaderInterface::ILoaderAdapter* AWorldPartitionVolume::GetLoaderAdapter()
{
	return WorldPartitionActorLoader;
}
#endif

#undef LOCTEXT_NAMESPACE