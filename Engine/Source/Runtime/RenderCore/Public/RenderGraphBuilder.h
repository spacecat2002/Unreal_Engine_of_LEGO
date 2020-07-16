// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphDefinitions.h"
#include "RenderGraphEvent.h"
#include "RenderGraphResources.h"
#include "RenderGraphPass.h"
#include "RenderGraphValidation.h"
#include "ShaderParameterMacros.h"
#include "ProfilingDebugging/CsvProfiler.h"

class FRDGLogFile;

/** Builds the per-frame render graph.
 *  Resources must be created from the builder before they can be bound to Pass ResourceTables.
 *  These resources are descriptors only until the graph is executed, where RHI resources are allocated as needed.
 */
class RENDERCORE_API FRDGBuilder
{
public:
	FRDGBuilder(FRHICommandListImmediate& InRHICmdList, FRDGEventName InName = {}, const char* UnaccountedCSVStat = kDefaultUnaccountedCSVStat);
	FRDGBuilder(const FRDGBuilder&) = delete;

	/** Finds an RDG texture associated with the external texture, or returns null if none is found. */
	FRDGTextureRef FindExternalTexture(FRHITexture* Texture) const;
	FRDGTextureRef FindExternalTexture(IPooledRenderTarget* ExternalPooledTexture, ERenderTargetTexture Texture) const;

	/** Register a external texture to be tracked by the render graph. */
	FRDGTextureRef RegisterExternalTexture(
		const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture,
		ERenderTargetTexture Texture = ERenderTargetTexture::ShaderResource,
		ERDGTextureFlags Flags = ERDGTextureFlags::None);

	/** Register an external texture with a custom name. The name is only used if this is the first registration of the texture. */
	FRDGTextureRef RegisterExternalTexture(
		const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture,
		const TCHAR* NameIfNotRegistered,
		ERenderTargetTexture RenderTargetTexture = ERenderTargetTexture::ShaderResource,
		ERDGTextureFlags Flags = ERDGTextureFlags::None);

	/** Register a external buffer to be tracked by the render graph. */
	FRDGBufferRef RegisterExternalBuffer(const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer, ERDGBufferFlags Flags = ERDGBufferFlags::None);
	FRDGBufferRef RegisterExternalBuffer(const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer, ERDGBufferFlags Flags, ERHIAccess AccessFinal);

	/** Register an external buffer with a custom name. The name is only used if this is the first registration of the buffer. */
	FRDGBufferRef RegisterExternalBuffer(
		const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer,
		const TCHAR* NameIfNotRegistered,
		ERDGBufferFlags Flags = ERDGBufferFlags::None);

	/** Create graph tracked resource from a descriptor with a debug name.
	 *
	 *  The debug name is the name used for GPU debugging tools, but also for the VisualizeTexture/Vis command.
	 */
	FRDGTextureRef CreateTexture(const FRDGTextureDesc& Desc, const TCHAR* Name, ERDGTextureFlags Flags = ERDGTextureFlags::None);

	/** Create graph tracked resource from a descriptor with a debug name.
	 *
	 *  The debug name is the name used for GPU debugging tools, but also for the VisualizeTexture/Vis command.
	 */
	FRDGBufferRef CreateBuffer(const FRDGBufferDesc& Desc, const TCHAR* Name, ERDGBufferFlags Flags = ERDGBufferFlags::None);

	/** Create graph tracked SRV for a texture from a descriptor. */
	FRDGTextureSRVRef CreateSRV(const FRDGTextureSRVDesc& Desc);

	/** Create graph tracked SRV for a buffer from a descriptor. */
	FRDGBufferSRVRef CreateSRV(const FRDGBufferSRVDesc& Desc);

	FORCEINLINE FRDGBufferSRVRef CreateSRV(FRDGBufferRef Buffer, EPixelFormat Format)
	{
		return CreateSRV(FRDGBufferSRVDesc(Buffer, Format));
	}

	/** Create graph tracked UAV for a texture from a descriptor. */
	FRDGTextureUAVRef CreateUAV(const FRDGTextureUAVDesc& Desc, ERDGUnorderedAccessViewFlags Flags = ERDGUnorderedAccessViewFlags::None);

	FORCEINLINE FRDGTextureUAVRef CreateUAV(FRDGTextureRef Texture, ERDGUnorderedAccessViewFlags Flags = ERDGUnorderedAccessViewFlags::None)
	{
		return CreateUAV(FRDGTextureUAVDesc(Texture), Flags);
	}

	/** Create graph tracked UAV for a buffer from a descriptor. */
	FRDGBufferUAVRef CreateUAV(const FRDGBufferUAVDesc& Desc, ERDGUnorderedAccessViewFlags Flags = ERDGUnorderedAccessViewFlags::None);

	FORCEINLINE FRDGBufferUAVRef CreateUAV(FRDGBufferRef Buffer, EPixelFormat Format, ERDGUnorderedAccessViewFlags Flags = ERDGUnorderedAccessViewFlags::None)
	{
		return CreateUAV(FRDGBufferUAVDesc(Buffer, Format), Flags);
	}

	/** Allocates raw memory using an allocator tied to the lifetime of the graph. */
	void* Alloc(uint32 SizeInBytes, uint32 AlignInBytes);

	/** Allocates POD memory using an allocator tied to the lifetime of the graph. Does not construct / destruct. */
	template <typename PODType>
	PODType* AllocPOD();

	/** Allocates a C++ object using an allocator tied to the lifetime of the graph. Will destruct the object. */
	template <typename ObjectType, typename... TArgs>
	ObjectType* AllocObject(TArgs&&... Args);

	/** Allocates a parameter struct with a lifetime tied to graph execution. */
	template <typename ParameterStructType>
	ParameterStructType* AllocParameters();

	/** Creates a graph tracked uniform buffer which can be attached to passes. */
	template <typename ParameterStructType>
	TRDGUniformBufferRef<ParameterStructType> CreateUniformBuffer(ParameterStructType* ParameterStruct, const TCHAR* Name);

	/** Adds a lambda pass to the graph.
	 *
	 * The Name of the pass should be generated with enough information to identify it's purpose and GPU cost, to be clear
	 * for GPU profiling tools.
	 *
	 * Caution: The pass parameter will be validated, and should not longer be modified after this call, since the pass may be executed
	 * right away with the immediate debugging mode.
	 */
	template <typename ParameterStructType, typename ExecuteLambdaType>
	FRDGPassRef AddPass(FRDGEventName&& Name, ParameterStructType* ParameterStruct, ERDGPassFlags Flags, ExecuteLambdaType&& ExecuteLambda);

	/** Adds a lambda pass to the graph without any parameters. This useful for deferring RHI work onto the graph timeline,
	 *  or incrementally porting code to use the graph system. NeverCull and SkipRenderPass (if Raster) are implicitly added
	 *  to Flags. AsyncCompute is not allowed. Accessing RHI resources which are tracked by the graph is only allowed if the
	 *  UntrackedAccess flag is specified. It is never permitted to access a created (i.e. not externally registered) RDG resource
	 *  in an independent pass, as the RHI lifetime is not guaranteed.
	 */
	template <typename ExecuteLambdaType>
	FRDGPassRef AddPass(FRDGEventName&& Name, ERDGPassFlags Flags, ExecuteLambdaType&& ExecuteLambda);

#if WITH_MGPU
	void SetNameForTemporalEffect(FName InNameForTemporalEffect)
	{
		NameForTemporalEffect = InNameForTemporalEffect;
	}
#endif

	/** Queues a pooled resource extraction to happen at the end of graph execution. *OutTexturePtr is assigned the internal
	 *  pooled render target. Extractions are still deferred in immediate mode to preserve expected behavior.
	 */
	void QueueTextureExtraction(FRDGTextureRef Texture, TRefCountPtr<IPooledRenderTarget>* OutPooledTexturePtr);
	void QueueTextureExtraction(FRDGTextureRef Texture, TRefCountPtr<IPooledRenderTarget>* OutPooledTexturePtr, ERHIAccess AccessFinal);

	UE_DEPRECATED(4.26, "QueueTextureExtraction with bTransitionToRead is deprecated; use the ERHIAccess variant instead.")
	void QueueTextureExtraction(FRDGTextureRef Texture, TRefCountPtr<IPooledRenderTarget>* OutTexturePtr, bool bTransitionToRead);

	void QueueBufferExtraction(FRDGBufferRef Buffer, TRefCountPtr<FRDGPooledBuffer>* OutPooledBufferPtr);
	void QueueBufferExtraction(FRDGBufferRef Buffer, TRefCountPtr<FRDGPooledBuffer>* OutPooledBufferPtr, ERHIAccess AccessFinal);

	/** If the resource is created by the graph, this will force allocation of the underlying resource in order to allow immediate extraction. */
	void PreallocateTexture(FRDGTextureRef Texture);
	void PreallocateBuffer(FRDGBufferRef Buffer);

	/** Performs an immediate query for the underlying pooled texture. This is only allowed for registered or preallocated textures. */
	const TRefCountPtr<IPooledRenderTarget>& GetPooledTexture(FRDGTextureRef Texture) const;

	/** Performs an immediate query for the underlying pooled buffer. This is only allowed for registered or preallocated buffers. */
	const TRefCountPtr<FRDGPooledBuffer>& GetPooledBuffer(FRDGBufferRef Buffer) const;

	/** Sets the access to transition to after execution. Only valid on external or extracted textures. Overwrites any previously set final access. */
	void SetTextureAccessFinal(FRDGTextureRef Texture, ERHIAccess Access);

	/** Sets the access to transition to after execution. Only valid on external or extracted buffers. Overwrites any previously set final access. */
	void SetBufferAccessFinal(FRDGBufferRef Buffer, ERHIAccess Access);

	/** Flag a texture that is produced by a pass but never used or extracted to not emit an 'unused' warning. */
	void RemoveUnusedTextureWarning(FRDGTextureRef Texture);

	/** Flag a buffer that is produced by a pass but never used or extracted to not emit an 'unused' warning. */
	void RemoveUnusedBufferWarning(FRDGBufferRef Buffer);

	/** Executes the queued passes, managing setting of render targets (RHI RenderPasses), resource transitions and queued texture extraction. */
	void Execute();

	/** Per-frame update of the render graph resource pool. */
	static void TickPoolElements();

	const FRHIGPUMask GetGPUMask() const
	{
		return GpuMaskScope;
	}

	/** The RHI command list used for the render graph. */
	FRHICommandListImmediate& RHICmdList;

private:
	static const ERHIAccess kDefaultAccessInitial = ERHIAccess::Unknown;
	static const ERHIAccess kDefaultAccessFinal = ERHIAccess::SRVMask;
	static const char* const kDefaultUnaccountedCSVStat;

	FRHIAsyncComputeCommandListImmediate& RHICmdListAsyncCompute;
	FRDGAllocator Allocator;

	const FRDGEventName BuilderName;

	ERDGPassFlags OverridePassFlags(const TCHAR* PassName, ERDGPassFlags Flags, bool bAsyncComputeSupported);

	FORCEINLINE FRDGPassHandle GetProloguePassHandle() const
	{
		return Passes.Begin();
	}

	FORCEINLINE FRDGPassHandle GetEpiloguePassHandle() const
	{
		checkf(EpiloguePass, TEXT("The handle is not valid until the epilogue has been added to the graph during execution."));
		return Passes.Last();
	}

	/** Registry of graph objects. */
	FRDGPassRegistry Passes;
	FRDGTextureRegistry Textures;
	FRDGBufferRegistry Buffers;
	FRDGViewRegistry Views;
	FRDGUniformBufferRegistry UniformBuffers;

	/** Passes that have been culled from the graph. */
	FRDGPassBitArray PassesToCull;

	/** Passes that don't have any parameters. */
	FRDGPassBitArray PassesWithEmptyParameters;

	/** Tracks external resources to their registered render graph counterparts for de-duplication. */
	TSortedMap<FRHITexture*, FRDGTexture*, TInlineAllocator<4, SceneRenderingAllocator>> ExternalTextures;
	TSortedMap<const FRDGPooledBuffer*, FRDGBuffer*, TInlineAllocator<4, SceneRenderingAllocator>> ExternalBuffers;

	/** The epilogue and prologue passes are sentinels that are used to simplify graph logic around barriers
	 *  and traversal. The prologue pass is used exclusively for barriers before the graph executes, while the
	 *  epilogue pass is used for resource extraction barriers--a property that also makes it the main root of
	 *  the graph for culling purposes. The epilogue pass is added to the very end of the pass array for traversal
	 *  purposes. The prologue does not need to participate in any graph traversal behavior.
	 */
	FRDGPass* ProloguePass = nullptr;
	FRDGPass* EpiloguePass = nullptr;

	/** Array of all requested resource extractions from the graph. */
	TArray<TPair<FRDGTextureRef, TRefCountPtr<IPooledRenderTarget>*>, TInlineAllocator<4, SceneRenderingAllocator>> ExtractedTextures;
	TArray<TPair<FRDGBufferRef, TRefCountPtr<FRDGPooledBuffer>*>, TInlineAllocator<4, SceneRenderingAllocator>> ExtractedBuffers;

	FRHIGPUMask GpuMaskScope;
	IF_RDG_CPU_SCOPES(FRDGCPUScopeStacks CPUScopeStacks);
	IF_RDG_GPU_SCOPES(FRDGGPUScopeStacksByPipeline GPUScopeStacks);

#if RDG_ENABLE_DEBUG
	FRDGUserValidation UserValidation;
	FRDGBarrierValidation BarrierValidation;
	FRDGLogFile LogFile;

	/** Tracks whether we are in a scope of adding passes to the builder. Used to avoid recursion. */
	bool bInDebugPassScope = false;
#endif

#if WITH_MGPU
	/** Name for the temporal effect used to synchronize multi-frame resources. */
	FName NameForTemporalEffect;

	/** Whether we performed the wait for the temporal effect yet. */
	bool bWaitedForTemporalEffect = false;
#endif

	void Compile();
	void Clear();

	void BeginResourceRHI(FRDGUniformBuffer* UniformBuffer);
	void BeginResourceRHI(FRDGPassHandle, FRDGTexture* Texture);
	void BeginResourceRHI(FRDGPassHandle, FRDGTextureSRV* SRV);
	void BeginResourceRHI(FRDGPassHandle, FRDGTextureUAV* UAV);
	void BeginResourceRHI(FRDGPassHandle, FRDGBuffer* Buffer);
	void BeginResourceRHI(FRDGPassHandle, FRDGBufferSRV* SRV);
	void BeginResourceRHI(FRDGPassHandle, FRDGBufferUAV* UAV);

	void EndResourceRHI(FRDGPassHandle, FRDGTexture* Texture, const FRDGTextureSubresourceRange Range);
	void EndResourceRHI(FRDGPassHandle, FRDGBuffer* Buffer);

	void SetupPassInternal(FRDGPass* Pass, FRDGPassHandle PassHandle, ERHIPipeline PassPipeline);
	void SetupPass(FRDGPass* Pass);
	void SetupEmptyPass(FRDGPass* Pass);
	void ExecutePass(FRDGPass* Pass);

	void ExecutePassPrologue(FRHIComputeCommandList& RHICmdListPass, FRDGPass* Pass);
	void ExecutePassEpilogue(FRHIComputeCommandList& RHICmdListPass, FRDGPass* Pass);

	void CollectPassResources(FRDGPassHandle PassHandle);
	void CollectPassBarriers(FRDGPassHandle PassHandle, FRDGPassHandle& LastUntrackedPassHandle);

	void AddPassDependency(FRDGPassHandle ProducerHandle, FRDGPassHandle ConsumerHandle);

	void AddEpilogueTransition(FRDGTextureRef Texture, FRDGPassHandle LastUntrackedPassHandle);
	void AddEpilogueTransition(FRDGBufferRef Buffer, FRDGPassHandle LastUntrackedPassHandle);

	void AddTransition(FRDGPassHandle PassHandle, FRDGTextureRef Texture, const FRDGTextureState& StateAfter, FRDGPassHandle LastUntrackedPassHandle);
	void AddTransition(FRDGPassHandle PassHandle, FRDGBufferRef Buffer, FRDGSubresourceState StateAfter, FRDGPassHandle LastUntrackedPassHandle);

	void AddTransitionInternal(
		FRDGParentResource* Resource,
		FRDGSubresourceState StateBefore,
		FRDGSubresourceState StateAfter,
		FRDGPassHandle LastUntrackedPassHandle,
		const FRHITransitionInfo& TransitionInfo);

	FRHIRenderPassInfo GetRenderPassInfo(const FRDGPass* Pass) const;

#if RDG_ENABLE_DEBUG
	void VisualizePassOutputs(const FRDGPass* Pass);
	void ClobberPassOutputs(const FRDGPass* Pass);
#endif

	friend FRDGEventScopeGuard;
	friend FRDGGPUStatScopeGuard;
	friend FRDGGPUMaskScopeGuard;
	friend FRDGScopedCsvStatExclusive;
	friend FRDGScopedCsvStatExclusiveConditional;
};

class FRDGGPUMaskScopeGuard final
{
public:
	FRDGGPUMaskScopeGuard(FRDGBuilder& InGraphBuilder, FRHIGPUMask InGpuMask)
		: GraphBuilder(InGraphBuilder)
		, GpuMaskRestore(GraphBuilder.GpuMaskScope)
	{
		GraphBuilder.GpuMaskScope = InGpuMask;
	}

	~FRDGGPUMaskScopeGuard()
	{
		GraphBuilder.GpuMaskScope = GpuMaskRestore;
	}

private:
	FRDGBuilder& GraphBuilder;
	FRHIGPUMask GpuMaskRestore;
};

#if WITH_MGPU
	#define RDG_GPU_MASK_SCOPE(GraphBuilder, GPUMask) FRDGGPUMaskScopeGuard PREPROCESSOR_JOIN(RDGGPUMaskScope, __LINE__){ GraphBuilder, GPUMask }
#else
	#define RDG_GPU_MASK_SCOPE(GraphBuilder, GPUMask)
#endif

#include "RenderGraphBuilder.inl"