// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Misc/AssetRegistryInterface.h"
#include "IAssetRegistry.h"

namespace AssetRegistryConstants
{
	const FName ModuleName("AssetRegistry");
}

/**
 * Asset registry module
 */
class FAssetRegistryModule : public IAssetRegistryInterface
{

public:

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override;

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override;

	/** Gets the asset registry singleton */
	virtual IAssetRegistry& Get() const;

	/** Gets the asset registry singleton */
	static IAssetRegistry& GetRegistry()
	{
		static FName ModuleName("AssetRegistry");
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(ModuleName);
		return AssetRegistryModule.Get();
	}

	/** Tick the asset registry with the supplied timestep */
	static void TickAssetRegistry(float DeltaTime)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		AssetRegistryModule.Get().Tick(DeltaTime);
	}

	/** Notifies the asset registry of a new in-memory asset */
	static void AssetCreated(UObject* NewAsset)
	{
		if ( GIsEditor )
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
			AssetRegistryModule.Get().AssetCreated(NewAsset);
		}
	}

	/** Notifies the asset registry than an in-memory asset was deleted */
	static void AssetDeleted(UObject* DeletedAsset)
	{
		if ( GIsEditor )
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
			AssetRegistryModule.Get().AssetDeleted(DeletedAsset);
		}
	}

	/** Notifies the asset registry that an in-memory asset was renamed */
	static void AssetRenamed(const UObject* RenamedAsset, const FString& OldObjectPath)
	{
		if ( GIsEditor )
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
			AssetRegistryModule.Get().AssetRenamed(RenamedAsset, OldObjectPath);
		}
	}

	/** Notifies the asset registry that an in-memory package was deleted */
	static void PackageDeleted(UPackage* DeletedPackage)
	{
		if (GIsEditor)
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
			AssetRegistryModule.Get().PackageDeleted(DeletedPackage);
		}
	}

	UE_DEPRECATED(4.26, "Use GetDependencies that takes a UE::AssetRegistry::EDependencyCategory instead")
	void GetDependencies(FName InPackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType)
	{
		GetDependenciesDeprecated(InPackageName, OutDependencies, InDependencyType);
	}

	/** Access the dependent package names for a given source package */
	virtual void GetDependencies(FName InPackageName, TArray<FName>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::Package, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) override
	{
		Get().GetDependencies(InPackageName, OutDependencies, Category, Flags);
	}


protected:
	/* This function is a workaround for platforms that don't support disable of deprecation warnings on override functions*/
	virtual void GetDependenciesDeprecated(FName InPackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) override
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Get().GetDependencies(InPackageName, OutDependencies, InDependencyType);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
private:

	TWeakObjectPtr<class UAssetRegistryImpl> AssetRegistry;
	class FAssetRegistryConsoleCommands* ConsoleCommands;
};
