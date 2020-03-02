// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalDynamicRHI.cpp: Metal Dynamic RHI Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "MetalShaderTypes.h"
#include "MetalVertexDeclaration.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalComputePipelineState.h"


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Vertex Declaration Methods


FVertexDeclarationRHIRef FMetalDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	@autoreleasepool {
		uint32 Key = FCrc::MemCrc32(Elements.GetData(), Elements.Num() * sizeof(FVertexElement));

		// look up an existing declaration
		FVertexDeclarationRHIRef* VertexDeclarationRefPtr = VertexDeclarationCache.Find(Key);
		if (VertexDeclarationRefPtr == NULL)
		{
			// create and add to the cache if it doesn't exist.
			VertexDeclarationRefPtr = &VertexDeclarationCache.Add(Key, new FMetalVertexDeclaration(Elements));
		}

		return *VertexDeclarationRefPtr;
	} // autoreleasepool
}


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Pipeline State Methods


FGraphicsPipelineStateRHIRef FMetalDynamicRHI::RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	@autoreleasepool {
		FMetalGraphicsPipelineState* State = new FMetalGraphicsPipelineState(Initializer);

		if(!State->Compile())
		{
			// Compilation failures are propagated up to the caller.
			State->DoNoDeferDelete();
			delete State;
			return nullptr;
		}

		State->VertexDeclaration = ResourceCast(Initializer.BoundShaderState.VertexDeclarationRHI);
		State->VertexShader = ResourceCast(Initializer.BoundShaderState.VertexShaderRHI);
		State->PixelShader = ResourceCast(Initializer.BoundShaderState.PixelShaderRHI);
#if PLATFORM_SUPPORTS_TESSELLATION_SHADERS
		State->HullShader = ResourceCast(Initializer.BoundShaderState.HullShaderRHI);
		State->DomainShader = ResourceCast(Initializer.BoundShaderState.DomainShaderRHI);
#endif // PLATFORM_SUPPORTS_TESSELLATION_SHADERS
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		State->GeometryShader = ResourceCast(Initializer.BoundShaderState.GeometryShaderRHI);
#endif // PLATFORM_SUPPORTS_GEOMETRY_SHADERS

		State->DepthStencilState = ResourceCast(Initializer.DepthStencilState);
		State->RasterizerState = ResourceCast(Initializer.RasterizerState);

		return State;
	} // autoreleasepool
}

TRefCountPtr<FRHIComputePipelineState> FMetalDynamicRHI::RHICreateComputePipelineState(FRHIComputeShader* ComputeShader)
{
	@autoreleasepool {
		return new FMetalComputePipelineState(ResourceCast(ComputeShader));
	} // autoreleasepool
}
