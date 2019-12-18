// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Animation/AnimCompressionDerivedData.h"
#include "Stats/Stats.h"
#include "Animation/AnimSequence.h"
#include "Serialization/MemoryWriter.h"
#include "AnimationUtils.h"
#include "AnimEncoding.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimCurveCompressionSettings.h"
#include "AnimationCompression.h"
#include "UObject/Package.h"

#if WITH_EDITOR

DECLARE_CYCLE_STAT(TEXT("Anim Compression (Derived Data)"), STAT_AnimCompressionDerivedData, STATGROUP_Anim);

FDerivedDataAnimationCompression::FDerivedDataAnimationCompression(const TCHAR* InTypeName, const FString& InAssetDDCKey, TSharedPtr<FAnimCompressContext> InCompressContext, int32 InPreviousCompressedSize)
	: TypeName(InTypeName)
	, AssetDDCKey(InAssetDDCKey)
	, CompressContext(InCompressContext)
	, PreviousCompressedSize(InPreviousCompressedSize)
{

}

FDerivedDataAnimationCompression::~FDerivedDataAnimationCompression()
{
}

const TCHAR* FDerivedDataAnimationCompression::GetVersionString() const
{
	// This is a version string that mimics the old versioning scheme. If you
	// want to bump this version, generate a new guid using VS->Tools->Create GUID and
	// return it here. Ex.
	return TEXT("EFB2D82D0DDA4A4E9D0D9AC41A9C557A");
}

bool FDerivedDataAnimationCompression::Build( TArray<uint8>& OutDataArray )
{
	check(DataToCompressPtr.IsValid());
	FCompressibleAnimData& DataToCompress = *DataToCompressPtr.Get();
	FCompressedAnimSequence OutData;

	SCOPE_CYCLE_COUNTER(STAT_AnimCompressionDerivedData);
	UE_LOG(LogAnimationCompression, Log, TEXT("Building Anim DDC data for %s"), *DataToCompress.FullName);

	FCompressibleAnimDataResult CompressionResult;

	bool bCompressionSuccessful = false;
	{
		DataToCompress.Update(OutData);

		const bool bBoneCompressionOk = FAnimationUtils::CompressAnimBones(DataToCompress, CompressionResult);
		const bool bCurveCompressionOk = FAnimationUtils::CompressAnimCurves(DataToCompress, OutData);

#if DO_CHECK
		FString CompressionName = DataToCompress.BoneCompressionSettings->GetFullName();
		const TCHAR* AAC = CompressContext.Get()->bAllowAlternateCompressor ? TEXT("true") : TEXT("false");
		const TCHAR* OutputStr = CompressContext.Get()->bOutput ? TEXT("true") : TEXT("false");
#endif

		bCompressionSuccessful = bBoneCompressionOk && bCurveCompressionOk;

		ensureMsgf(bCompressionSuccessful, TEXT("Anim Compression failed for Sequence '%s' with compression scheme '%s': compressed data empty\n\tAnimIndex: %i\n\tMaxAnim:%i\n\tAllowAltCompressor:%s\n\tOutput:%s"), 
											*DataToCompress.FullName,
											*CompressionName,
											CompressContext.Get()->AnimIndex,
											CompressContext.Get()->MaxAnimations,
											AAC,
											OutputStr);
	}

	if (bCompressionSuccessful)
	{
		OutData.CompressedByteStream = MoveTemp(CompressionResult.CompressedByteStream);
		OutData.CompressedDataStructure = MoveTemp(CompressionResult.AnimData);
		OutData.BoneCompressionCodec = CompressionResult.Codec;

		FMemoryWriter Ar(OutDataArray, true);
		OutData.SerializeCompressedData(Ar, true, nullptr, DataToCompress.Skeleton, DataToCompress.BoneCompressionSettings, DataToCompress.CurveCompressionSettings); //Save out compressed
	}

	return bCompressionSuccessful;
}
#endif	//WITH_EDITOR
