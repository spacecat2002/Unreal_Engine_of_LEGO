// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalDynamicRHI_PipelineStates.cpp:
		Metal Dynamic RHI Class Pipeline State Methods.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalComputePipelineState.h"


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
