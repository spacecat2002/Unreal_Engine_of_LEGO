// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaders.cpp: Metal shader RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "Shaders/Debugging/MetalShaderDebugCache.h"
#include "Shaders/MetalCompiledShaderKey.h"
#include "Shaders/MetalCompiledShaderCache.h"
#include "Shaders/MetalShaderLibrary.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "MetalShaderResources.h"
#include "MetalResources.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/Compression.h"
#include "Misc/MessageDialog.h"

#define SHADERCOMPILERCOMMON_API
#	include "Developer/ShaderCompilerCommon/Public/ShaderCompilerCommon.h"
#undef SHADERCOMPILERCOMMON_API

NSString* DecodeMetalSourceCode(uint32 CodeSize, TArray<uint8> const& CompressedSource)
{
	NSString* GlslCodeNSString = nil;
	if (CodeSize && CompressedSource.Num())
	{
		TArray<ANSICHAR> UncompressedCode;
		UncompressedCode.AddZeroed(CodeSize+1);
		bool bSucceed = FCompression::UncompressMemory(NAME_Zlib, UncompressedCode.GetData(), CodeSize, CompressedSource.GetData(), CompressedSource.Num());
		if (bSucceed)
		{
			GlslCodeNSString = [[NSString stringWithUTF8String:UncompressedCode.GetData()] retain];
		}
	}
	return GlslCodeNSString;
}

mtlpp::LanguageVersion ValidateVersion(uint8 Version)
{
	static uint32 MetalMacOSVersions[][3] = {
		{10,11,6},
		{10,11,6},
		{10,12,6},
		{10,13,0},
		{10,14,0},
	};
	static uint32 MetaliOSVersions[][3] = {
		{8,0,0},
		{9,0,0},
		{10,0,0},
		{11,0,0},
		{12,0,0},
	};
	static TCHAR const* StandardNames[] =
	{
		TEXT("Metal 1.0"),
		TEXT("Metal 1.1"),
		TEXT("Metal 1.2"),
		TEXT("Metal 2.0"),
		TEXT("Metal 2.1"),
	};
	
	mtlpp::LanguageVersion Result = mtlpp::LanguageVersion::Version1_1;
	switch(Version)
	{
		case 4:
			Result = mtlpp::LanguageVersion::Version2_1;
			break;
		case 3:
			Result = mtlpp::LanguageVersion::Version2_0;
			break;
		case 2:
			Result = mtlpp::LanguageVersion::Version1_2;
			break;
		case 1:
			Result = mtlpp::LanguageVersion::Version1_1;
			break;
		case 0:
		default:
#if PLATFORM_MAC
			Result = mtlpp::LanguageVersion::Version1_1;
#else
			Result = mtlpp::LanguageVersion::Version1_0;
#endif
			break;
	}
	
	if (!FApplePlatformMisc::IsOSAtLeastVersion(MetalMacOSVersions[Version], MetaliOSVersions[Version], MetaliOSVersions[Version]))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ShaderVersion"), FText::FromString(FString(StandardNames[Version])));
#if PLATFORM_MAC
		Args.Add(TEXT("RequiredOS"), FText::FromString(FString::Printf(TEXT("macOS %d.%d.%d"), MetalMacOSVersions[Version][0], MetalMacOSVersions[Version][1], MetalMacOSVersions[Version][2])));
#else
		Args.Add(TEXT("RequiredOS"), FText::FromString(FString::Printf(TEXT("%d.%d.%d"), MetaliOSVersions[Version][0], MetaliOSVersions[Version][1], MetaliOSVersions[Version][2])));
#endif
		FText LocalizedMsg = FText::Format(NSLOCTEXT("MetalRHI", "ShaderVersionUnsupported", "The current OS version does not support {ShaderVersion} required by the project. You must upgrade to {RequiredOS} to run this project."),Args);
		
		FText Title = NSLOCTEXT("MetalRHI", "ShaderVersionUnsupportedTitle", "Shader Version Unsupported");
		FMessageDialog::Open(EAppMsgType::Ok, LocalizedMsg, &Title);
		
		FPlatformMisc::RequestExit(true);
	}
	
	return Result;
}

FMetalShaderParameterCache::FMetalShaderParameterCache()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniforms[ArrayIndex] = nullptr;
		PackedGlobalUniformsSizes[ArrayIndex] = 0;
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;
	}
}

void FMetalShaderParameterCache::ResizeGlobalUniforms(uint32 TypeIndex, uint32 UniformArraySize)
{
	if (!PackedGlobalUniforms[TypeIndex])
	{
		PackedGlobalUniforms[TypeIndex] = [[FMetalBufferData alloc] initWithSize:UniformArraySize];
	}
	else
	{
		PackedGlobalUniforms[TypeIndex]->Data = (uint8*)FMemory::Realloc(PackedGlobalUniforms[TypeIndex]->Data, UniformArraySize);
		PackedGlobalUniforms[TypeIndex]->Len = UniformArraySize;
	}
	PackedGlobalUniformsSizes[TypeIndex] = UniformArraySize;
	PackedGlobalUniformDirty[TypeIndex].LowVector = 0;
	PackedGlobalUniformDirty[TypeIndex].HighVector = 0;
}

/** Destructor. */
FMetalShaderParameterCache::~FMetalShaderParameterCache()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		[PackedGlobalUniforms[ArrayIndex] release];
	}
}

/**
 * Invalidates all existing data.
 */
void FMetalShaderParameterCache::Reset()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;
	}
}

static const int SizeOfFloat = sizeof(float);

/**
 * Marks all uniform arrays as dirty.
 */
void FMetalShaderParameterCache::MarkAllDirty()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = PackedGlobalUniformsSizes[ArrayIndex] / SizeOfFloat;
	}
}

/**
 * Set parameter values.
 */
void FMetalShaderParameterCache::Set(uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValues)
{
	if (NumBytes)
	{
		uint32 BufferIndex = CrossCompiler::PackedTypeNameToTypeIndex(BufferIndexName);
		check(BufferIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
		check(PackedGlobalUniforms[BufferIndex]);
		check(ByteOffset + NumBytes <= PackedGlobalUniformsSizes[BufferIndex]);
		PackedGlobalUniformDirty[BufferIndex].LowVector = FMath::Min(PackedGlobalUniformDirty[BufferIndex].LowVector, ByteOffset / SizeOfFloat);
		PackedGlobalUniformDirty[BufferIndex].HighVector = FMath::Max(PackedGlobalUniformDirty[BufferIndex].HighVector, (ByteOffset + NumBytes + SizeOfFloat - 1) / SizeOfFloat);
		FMemory::Memcpy(PackedGlobalUniforms[BufferIndex]->Data + ByteOffset, NewValues, NumBytes);
	}
}

void FMetalShaderParameterCache::CommitPackedGlobals(FMetalStateCache* Cache, FMetalCommandEncoder* Encoder, uint32 Frequency, const FMetalShaderBindings& Bindings)
{
	// copy the current uniform buffer into the ring buffer to submit
	for (int32 Index = 0; Index < Bindings.PackedGlobalArrays.Num(); ++Index)
	{
		int32 UniformBufferIndex = Bindings.PackedGlobalArrays[Index].TypeIndex;
 
		// is there any data that needs to be copied?
		if (PackedGlobalUniformDirty[Index].HighVector > 0)
		{
			uint32 TotalSize = Bindings.PackedGlobalArrays[Index].Size;
			uint32 SizeToUpload = PackedGlobalUniformDirty[Index].HighVector * SizeOfFloat;
			
			//@todo-rco: Temp workaround
			SizeToUpload = TotalSize;
			
			//@todo-rco: Temp workaround
			uint32 Size = FMath::Min(TotalSize, SizeToUpload);
			if (Size > MetalBufferPageSize)
			{
				uint8 const* Bytes = PackedGlobalUniforms[Index]->Data;
				ns::AutoReleased<FMetalBuffer> Buffer(Encoder->GetRingBuffer().NewBuffer(Size, 0));
				FMemory::Memcpy((uint8*)Buffer.GetContents(), Bytes, Size);
				Cache->SetShaderBuffer((EMetalShaderStages)Frequency, Buffer, nil, 0, Size, UniformBufferIndex, mtlpp::ResourceUsage::Read);
			}
			else
			{
				PackedGlobalUniforms[Index]->Len = Size;
				Cache->SetShaderBuffer((EMetalShaderStages)Frequency, nil, nil, 0, 0, UniformBufferIndex, mtlpp::ResourceUsage(0));
				Cache->SetShaderBuffer((EMetalShaderStages)Frequency, nil, PackedGlobalUniforms[Index], 0, Size, UniformBufferIndex, mtlpp::ResourceUsage::Read);
			}

			// mark as clean
			PackedGlobalUniformDirty[Index].HighVector = 0;
		}
	}
}
