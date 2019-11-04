// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RenderGraphUtils.h"
#include "ClearQuad.h"
#include "ClearReplacementShaders.h"
#include "ShaderParameterUtils.h"
#include <initializer_list>
#include "GlobalShader.h"
#include "PixelShaderUtils.h"

/** Adds a render graph tracked buffer suitable for use as a copy destination. */
#define RDG_BUFFER_COPY_DEST(MemberName) \
	INTERNAL_SHADER_PARAMETER_EXPLICIT(UBMT_RDG_BUFFER_COPY_DEST, TShaderResourceParameterTypeInfo<FRDGBufferRef>, FRDGBufferRef,MemberName,, = nullptr,EShaderPrecisionModifier::Float,TEXT(""),false)

/** Adds a render graph tracked texture suitable for use as a copy destination. */
#define RDG_TEXTURE_COPY_DEST(MemberName) \
	INTERNAL_SHADER_PARAMETER_EXPLICIT(UBMT_RDG_TEXTURE_COPY_DEST, TShaderResourceParameterTypeInfo<FRDGTextureRef>, FRDGTextureRef,MemberName,, = nullptr,EShaderPrecisionModifier::Float,TEXT(""),false)

void ClearUnusedGraphResourcesImpl(
	const FShaderParameterBindings& ShaderBindings,
	const FShaderParametersMetadata* ParametersMetadata,
	void* InoutParameters,
	std::initializer_list<FRDGResourceRef> ExcludeList)
{
	int32 GraphTextureId = 0;
	int32 GraphSRVId = 0;
	int32 GraphUAVId = 0;

	uint8* Base = reinterpret_cast<uint8*>(InoutParameters);

	for (int32 ResourceIndex = 0, Num = ParametersMetadata->GetLayout().Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParametersMetadata->GetLayout().Resources[ResourceIndex].MemberType;
		uint16 ByteOffset = ParametersMetadata->GetLayout().Resources[ResourceIndex].MemberOffset;

		if (Type == UBMT_RDG_TEXTURE)
		{
			if (GraphTextureId < ShaderBindings.GraphTextures.Num() && ByteOffset == ShaderBindings.GraphTextures[GraphTextureId].ByteOffset)
			{
				GraphTextureId++;
				continue;
			}
		}
		else if (Type == UBMT_RDG_TEXTURE_SRV || Type == UBMT_RDG_BUFFER_SRV)
		{
			if (GraphSRVId < ShaderBindings.GraphSRVs.Num() && ByteOffset == ShaderBindings.GraphSRVs[GraphSRVId].ByteOffset)
			{
				GraphSRVId++;
				continue;
			}
		}
		else if (Type == UBMT_RDG_TEXTURE_UAV || Type == UBMT_RDG_BUFFER_UAV)
		{
			if (GraphUAVId < ShaderBindings.GraphUAVs.Num() && ByteOffset == ShaderBindings.GraphUAVs[GraphUAVId].ByteOffset)
			{
				GraphUAVId++;
				continue;
			}
		}
		else
		{
			continue;
		}

		for (FRDGResourceRef ExcludeResource : ExcludeList)
		{
			auto Resource = *reinterpret_cast<const FRDGResource* const*>(Base + ByteOffset);
			if (Resource == ExcludeResource)
			{
				continue;
			}
		}

		void** ResourcePointerAddress = reinterpret_cast<void**>(Base + ByteOffset);
		*ResourcePointerAddress = nullptr;
	}
}

void ClearUnusedGraphResourcesImpl(
	TArrayView<const FShaderParameterBindings*> ShaderBindingsList,
	const FShaderParametersMetadata* ParametersMetadata,
	void* InoutParameters,
	std::initializer_list<FRDGResourceRef> ExcludeList)
{
	TArray<int32, TInlineAllocator<SF_NumFrequencies>> GraphTextureIds;
	TArray<int32, TInlineAllocator<SF_NumFrequencies>> GraphSRVIds;
	TArray<int32, TInlineAllocator<SF_NumFrequencies>> GraphUAVIds;
	GraphTextureIds.SetNumZeroed(ShaderBindingsList.Num());
	GraphSRVIds.SetNumZeroed(ShaderBindingsList.Num());
	GraphUAVIds.SetNumZeroed(ShaderBindingsList.Num());

	uint8* Base = reinterpret_cast<uint8*>(InoutParameters);

	for (int32 ResourceIndex = 0, Num = ParametersMetadata->GetLayout().Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParametersMetadata->GetLayout().Resources[ResourceIndex].MemberType;
		uint16 ByteOffset = ParametersMetadata->GetLayout().Resources[ResourceIndex].MemberOffset;
		bool bResourceIsUsed = false;

		if (Type == UBMT_RDG_TEXTURE)
		{
			for (int32 Index = 0; Index < ShaderBindingsList.Num(); ++Index)
			{
				const FShaderParameterBindings& ShaderBindings = *ShaderBindingsList[Index];
				int32& GraphTextureId = GraphTextureIds[Index];

				if (GraphTextureId < ShaderBindings.GraphTextures.Num() && ByteOffset == ShaderBindings.GraphTextures[GraphTextureId].ByteOffset)
				{
					GraphTextureId++;
					bResourceIsUsed = true;
					break;
				}
			}
		}
		else if (Type == UBMT_RDG_TEXTURE_SRV || Type == UBMT_RDG_BUFFER_SRV)
		{
			for (int32 Index = 0; Index < ShaderBindingsList.Num(); ++Index)
			{
				const FShaderParameterBindings& ShaderBindings = *ShaderBindingsList[Index];
				int32& GraphSRVId = GraphSRVIds[Index];

				if (GraphSRVId < ShaderBindings.GraphSRVs.Num() && ByteOffset == ShaderBindings.GraphSRVs[GraphSRVId].ByteOffset)
				{
					GraphSRVId++;
					bResourceIsUsed = true;
					break;
				}
			}
		}
		else if (Type == UBMT_RDG_TEXTURE_UAV || Type == UBMT_RDG_BUFFER_UAV)
		{
			for (int32 Index = 0; Index < ShaderBindingsList.Num(); ++Index)
			{
				const FShaderParameterBindings& ShaderBindings = *ShaderBindingsList[Index];
				int32& GraphUAVId = GraphUAVIds[Index];

				if (GraphUAVId < ShaderBindings.GraphUAVs.Num() && ByteOffset == ShaderBindings.GraphUAVs[GraphUAVId].ByteOffset)
				{
					GraphUAVId++;
					bResourceIsUsed = true;
					break;
				}
			}
		}
		else
		{
			// Not a resource we care about.
			continue;
		}

		if (bResourceIsUsed)
		{
			continue;
		}

		for (FRDGResourceRef ExcludeResource : ExcludeList)
		{
			auto Resource = *reinterpret_cast<const FRDGResource* const*>(Base + ByteOffset);
			if (Resource == ExcludeResource)
			{
				continue;
			}
		}

		void** ResourcePointerAddress = reinterpret_cast<void**>(Base + ByteOffset);
		*ResourcePointerAddress = nullptr;
	}
}

FRDGTextureRef RegisterExternalTextureWithFallback(
	FRDGBuilder& GraphBuilder,
	const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture,
	const TRefCountPtr<IPooledRenderTarget>& FallbackPooledTexture,
	const TCHAR* ExternalPooledTextureName)
{
	ensureMsgf(FallbackPooledTexture.IsValid(), TEXT("RegisterExternalTextureWithDummyFallback() requires a valid fallback pooled texture."));
	if (ExternalPooledTexture.IsValid())
	{
		return GraphBuilder.RegisterExternalTexture(ExternalPooledTexture, ExternalPooledTextureName);
	}
	else
	{
		return GraphBuilder.RegisterExternalTexture(FallbackPooledTexture);
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FCopyTextureParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(, Input)
	RDG_TEXTURE_COPY_DEST(Output)
END_SHADER_PARAMETER_STRUCT()

void AddCopyTexturePass(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef InputTexture,
	FRDGTextureRef OutputTexture,
	const FRHICopyTextureInfo& CopyInfo)
{
	const FRDGTextureDesc& InputDesc = InputTexture->Desc;
	const FRDGTextureDesc& OutputDesc = OutputTexture->Desc;
	checkf(InputDesc.Format == OutputDesc.Format, TEXT("This method does not support format conversion."));

	FCopyTextureParameters* Parameters = GraphBuilder.AllocParameters<FCopyTextureParameters>();
	Parameters->Input = InputTexture;
	Parameters->Output = OutputTexture;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("CopyTexture(%s -> %s)", InputTexture->Name, OutputTexture->Name),
		Parameters,
		ERDGPassFlags::Copy,
		[InputTexture, OutputTexture, CopyInfo](FRHICommandList& RHICmdList)
	{
        // Manually mark as used since we aren't invoking any shaders.
		InputTexture->MarkResourceAsUsed();
		OutputTexture->MarkResourceAsUsed();

		RHICmdList.CopyTexture(InputTexture->GetRHI(), OutputTexture->GetRHI(), CopyInfo);
	});
}

BEGIN_SHADER_PARAMETER_STRUCT(FCopyToResolveTargetParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(, Input)
	RDG_TEXTURE_COPY_DEST(Output)
END_SHADER_PARAMETER_STRUCT()

void AddCopyToResolveTargetPass(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef InputTexture,
	FRDGTextureRef OutputTexture,
	const FResolveParams& ResolveParams)
{
	FCopyToResolveTargetParameters* Parameters = GraphBuilder.AllocParameters<FCopyToResolveTargetParameters>();
	Parameters->Input = InputTexture;
	Parameters->Output = OutputTexture;

	GraphBuilder.AddPass(RDG_EVENT_NAME("CopyTexture"), Parameters, ERDGPassFlags::Copy,
		[InputTexture, OutputTexture, ResolveParams](FRHICommandList& RHICmdList)
	{
		// Manually mark as used since we aren't invoking any shaders.
		InputTexture->MarkResourceAsUsed();
		OutputTexture->MarkResourceAsUsed();

		RHICmdList.CopyToResolveTarget(InputTexture->GetRHI(), OutputTexture->GetRHI(), ResolveParams);
	});
}

BEGIN_SHADER_PARAMETER_STRUCT(FClearBufferUAVParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, BufferUAV)
END_SHADER_PARAMETER_STRUCT()

void AddClearUAVPass(FRDGBuilder& GraphBuilder, FRDGBufferUAVRef BufferUAV, uint32 Value)
{
	FClearBufferUAVParameters* Parameters = GraphBuilder.AllocParameters<FClearBufferUAVParameters>();
	Parameters->BufferUAV = BufferUAV;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ClearBuffer(%s Size=%ubytes)", BufferUAV->GetParent()->Name, BufferUAV->GetParent()->Desc.GetTotalNumBytes()),
		Parameters,
		ERDGPassFlags::Compute,
		[&Parameters, BufferUAV, Value](FRHICommandList& RHICmdList)
	{
		// This might be called if using ClearTinyUAV.
		BufferUAV->MarkResourceAsUsed();

		::ClearUAV(RHICmdList, BufferUAV->GetRHI(), BufferUAV->GetParent()->Desc.GetTotalNumBytes(), Value);
	});
}

BEGIN_SHADER_PARAMETER_STRUCT(FClearTextureUAVParameters, )
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, TextureUAV)
END_SHADER_PARAMETER_STRUCT()

template <typename T>
void AddClearUAVPass_T(FRDGBuilder& GraphBuilder, FRDGTextureUAVRef TextureUAV, const T(&ClearValues)[4])
{
	check(TextureUAV);

	FClearTextureUAVParameters* Parameters = GraphBuilder.AllocParameters<FClearTextureUAVParameters>();
	Parameters->TextureUAV = TextureUAV;

	FRDGTextureRef Texture = TextureUAV->GetParent();

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ClearTexture(%s %s %dx%d Mip=%d)",
			Texture->Name,
			GPixelFormats[Texture->Desc.Format].Name,
			Texture->Desc.Extent.X, Texture->Desc.Extent.Y,
			int32(TextureUAV->Desc.MipLevel)),
		Parameters,
		ERDGPassFlags::Compute,
		[&Parameters, TextureUAV, ClearValues](FRHICommandList& RHICmdList)
	{
		const FRDGTextureDesc& LocalTextureDesc = TextureUAV->GetParent()->Desc;

		FRHIUnorderedAccessView* RHITextureUAV = TextureUAV->GetRHI();

		if (LocalTextureDesc.Is2DTexture())
		{
			if (LocalTextureDesc.IsArray())
			{
				typedef TClearReplacementCS<EClearReplacementResourceType::Texture2DArray, TClearReplacementBase<T, 4>> FClearShader;

				TShaderMapRef<FClearShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
				RHICmdList.SetComputeShader(ShaderRHI);
				SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->GetClearValueParam(), ClearValues);
				ComputeShader->SetResource(RHICmdList, RHITextureUAV);

				RHICmdList.DispatchComputeShader(
					FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Extent.X ), ComputeShader->ThreadGroupSizeX),
					FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Extent.Y ), ComputeShader->ThreadGroupSizeY),
					FMath::DivideAndRoundUp(uint32(LocalTextureDesc.ArraySize), ComputeShader->ThreadGroupSizeZ)
				);

				ComputeShader->FinalizeResource(RHICmdList, RHITextureUAV);
			}
			else
			{
				TShaderMapRef<TClearReplacementCS<EClearReplacementResourceType::Texture2D, TClearReplacementBase<T, 4>>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
				FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
				RHICmdList.SetComputeShader(ShaderRHI);
				SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->GetClearValueParam(), ClearValues);
				ComputeShader->SetResource(RHICmdList, RHITextureUAV);

				RHICmdList.DispatchComputeShader(
					FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Extent.X), ComputeShader->ThreadGroupSizeX),
					FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Extent.Y), ComputeShader->ThreadGroupSizeY),
					1
				);

				ComputeShader->FinalizeResource(RHICmdList, RHITextureUAV);
			}
		}
		else if (LocalTextureDesc.IsCubemap())
		{
			typedef TClearReplacementCS<EClearReplacementResourceType::Texture2DArray, TClearReplacementBase<T, 4>> FClearShader;

			TShaderMapRef<FClearShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
			RHICmdList.SetComputeShader(ShaderRHI);
			SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->GetClearValueParam(), ClearValues);
			ComputeShader->SetResource(RHICmdList, RHITextureUAV);

			// One complete cube of 6 faces
			const uint32 NumSlices = 6;

			RHICmdList.DispatchComputeShader(
				FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Extent.X), ComputeShader->ThreadGroupSizeX),
				FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Extent.Y), ComputeShader->ThreadGroupSizeY),
				FMath::DivideAndRoundUp(uint32(NumSlices                ), ComputeShader->ThreadGroupSizeZ)
			);

			ComputeShader->FinalizeResource(RHICmdList, RHITextureUAV);
		}
		else if (LocalTextureDesc.Is3DTexture())
		{
			typedef TClearReplacementCS<EClearReplacementResourceType::Texture3D, TClearReplacementBase<T, 4>> FClearShader;

			TShaderMapRef<FClearShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
			RHICmdList.SetComputeShader(ShaderRHI);
			SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->GetClearValueParam(), ClearValues);
			ComputeShader->SetResource(RHICmdList, RHITextureUAV);

			RHICmdList.DispatchComputeShader(
				FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Extent.X), ComputeShader->ThreadGroupSizeX),
				FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Extent.Y), ComputeShader->ThreadGroupSizeY),
				FMath::DivideAndRoundUp(uint32(LocalTextureDesc.Depth   ), ComputeShader->ThreadGroupSizeZ)
			);

			ComputeShader->FinalizeResource(RHICmdList, RHITextureUAV);
		}
		else
		{
			check(0);
		}
	});
}

void AddClearUAVPass(FRDGBuilder& GraphBuilder, FRDGTextureUAVRef TextureUAV, const float(&ClearValues)[4])
{
	AddClearUAVPass_T(GraphBuilder, TextureUAV, ClearValues);
}

void AddClearUAVPass(FRDGBuilder& GraphBuilder, FRDGTextureUAVRef TextureUAV, const uint32(&ClearValues)[4])
{
	AddClearUAVPass_T(GraphBuilder, TextureUAV, ClearValues);
}

void AddClearUAVPass(FRDGBuilder& GraphBuilder, FRDGTextureUAVRef TextureUAV, const FLinearColor& ClearColor)
{
	AddClearUAVPass_T(GraphBuilder, TextureUAV, reinterpret_cast<const float(&)[4]>(ClearColor));
}

class FClearUAVRectsPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearUAVRectsPS);
	SHADER_USE_PARAMETER_STRUCT(FClearUAVRectsPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FUintVector4, ClearValue)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ClearResource)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_CLEAR_VALUE"), 1);
		OutEnvironment.SetDefine(TEXT("RESOURCE_TYPE"), 1);
		OutEnvironment.SetDefine(TEXT("VALUE_TYPE"), TEXT("uint4"));
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearUAVRectsPS, "/Engine/Private/ClearReplacementShaders.usf", "ClearTextureRWPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FClearUAVRectsParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FClearUAVRectsPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void AddClearUAVPass(FRDGBuilder& GraphBuilder, FRDGTextureUAVRef TextureUAV, const uint32(&ClearValues)[4], FRDGBufferSRVRef RectMinMaxBufferSRV, uint32 NumRects)
{
	if (NumRects == 0)
	{
		AddClearUAVPass(GraphBuilder, TextureUAV, ClearValues);
		return;
	}

	check(TextureUAV && RectMinMaxBufferSRV);

	FClearUAVRectsParameters* PassParameters = GraphBuilder.AllocParameters<FClearUAVRectsParameters>();

	PassParameters->PS.ClearValue.X = ClearValues[0];
	PassParameters->PS.ClearValue.Y = ClearValues[1];
	PassParameters->PS.ClearValue.Z = ClearValues[2];
	PassParameters->PS.ClearValue.W = ClearValues[3];
	PassParameters->PS.ClearResource = TextureUAV;

	auto* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	auto PixelShader = ShaderMap->GetShader<FClearUAVRectsPS>();

	const FRDGTextureRef Texture = TextureUAV->GetParent();
	const FIntPoint TextureSize = Texture->Desc.Extent;

	FPixelShaderUtils::AddRasterizeToRectsPass<FClearUAVRectsPS>(GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("ClearTextureRects(%s %s %dx%d Mip=%d)",
			Texture->Name,
			GPixelFormats[Texture->Desc.Format].Name,
			Texture->Desc.Extent.X, Texture->Desc.Extent.Y,
			int32(TextureUAV->Desc.MipLevel)),
		PixelShader,
		PassParameters,
		TextureSize,
		RectMinMaxBufferSRV,
		NumRects,
		TStaticBlendState<>::GetRHI(),
		TStaticRasterizerState<>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always>::GetRHI());
}

void AddClearRenderTargetPass(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture, const FLinearColor& ClearColor)
{
	check(Texture);

	FRenderTargetParameters* Parameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
	Parameters->RenderTargets[0] = FRenderTargetBinding(Texture, ERenderTargetLoadAction::ENoAction);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ClearRenderTarget(%s) %dx%d", Texture->Name, Texture->Desc.Extent.X, Texture->Desc.Extent.Y),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, ClearColor](FRHICommandList& RHICmdList)
	{
		DrawClearQuad(RHICmdList, ClearColor);
	});
}

void AddClearDepthStencilPass(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef Texture,
	bool bClearDepth,
	float Depth,
	bool bClearStencil,
	uint8 Stencil)
{
	check(Texture);

	FExclusiveDepthStencil ExclusiveDepthStencil;
	ERenderTargetLoadAction DepthLoadAction = ERenderTargetLoadAction::ELoad;
	ERenderTargetLoadAction StencilLoadAction = ERenderTargetLoadAction::ENoAction;

	const bool bHasStencil = Texture->Desc.Format == PF_DepthStencil;

	// We can't clear stencil if we don't have it.
	bClearStencil &= bHasStencil;

	if (bClearDepth)
	{
		ExclusiveDepthStencil.SetDepthWrite();
		DepthLoadAction = ERenderTargetLoadAction::ENoAction;
	}

	if (bHasStencil)
	{
		if (bClearStencil)
		{
			ExclusiveDepthStencil.SetStencilWrite();
			StencilLoadAction = ERenderTargetLoadAction::ENoAction;
		}
		else
		{
			// Preserve stencil contents.
			StencilLoadAction = ERenderTargetLoadAction::ELoad;
		}
	}

	FRenderTargetParameters* Parameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
	Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(Texture, DepthLoadAction, StencilLoadAction, ExclusiveDepthStencil);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ClearDepthStencil(%s) %dx%d", Texture->Name, Texture->Desc.Extent.X, Texture->Desc.Extent.Y),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, bClearDepth, Depth, bClearStencil, Stencil](FRHICommandList& RHICmdList)
	{
		DrawClearQuad(RHICmdList, false, FLinearColor(), bClearDepth, Depth, bClearStencil, Stencil);
	});
}


class FClearUAVUIntCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearUAVUIntCS)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, UAV)
		SHADER_PARAMETER(uint32, ClearValue)
		SHADER_PARAMETER(uint32, NumEntries)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5;
	}

	FClearUAVUIntCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(this, Initializer.ParameterMap, *FParameters::FTypeInfo::GetStructMetadata());
	}

	FClearUAVUIntCS()
	{
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearUAVUIntCS, "/Engine/Private/Tools/ClearUAV.usf", "ClearUAVUIntCS", SF_Compute);

void FComputeShaderUtils::ClearUAV(FRDGBuilder& GraphBuilder, TShaderMap<FGlobalShaderType>* ShaderMap, FRDGBufferUAVRef UAV, uint32 ClearValue)
{
	FClearUAVUIntCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearUAVUIntCS::FParameters>();
	PassParameters->UAV = UAV;
	PassParameters->ClearValue = ClearValue;
	check(UAV->Desc.Format == PF_R32_UINT);
	PassParameters->NumEntries = UAV->Desc.Buffer->Desc.NumElements;
	check(PassParameters->NumEntries > 0);

	auto ComputeShader = ShaderMap->GetShader<FClearUAVUIntCS>();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ClearUAV"),
		ComputeShader,
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp<int32>(PassParameters->NumEntries, 64), 1, 1));
}

class FClearUAVFloatCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearUAVFloatCS)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, UAVFloat)
		SHADER_PARAMETER(FVector4, ClearValueFloat)
		SHADER_PARAMETER(uint32, NumEntries)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5;
	}

	FClearUAVFloatCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(this, Initializer.ParameterMap, *FParameters::FTypeInfo::GetStructMetadata());
	}

	FClearUAVFloatCS()
	{
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearUAVFloatCS, "/Engine/Private/Tools/ClearUAV.usf", "ClearUAVFloatCS", SF_Compute);

void FComputeShaderUtils::ClearUAV(FRDGBuilder& GraphBuilder, TShaderMap<FGlobalShaderType>* ShaderMap, FRDGBufferUAVRef UAV, FVector4 ClearValue)
{
	FClearUAVFloatCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearUAVFloatCS::FParameters>();
	PassParameters->UAVFloat = UAV;
	PassParameters->ClearValueFloat = ClearValue;
	check(UAV->Desc.Format == PF_A32B32G32R32F || UAV->Desc.Format == PF_FloatRGBA);
	PassParameters->NumEntries = UAV->Desc.Buffer->Desc.NumElements;
	check(PassParameters->NumEntries > 0);

	auto ComputeShader = ShaderMap->GetShader<FClearUAVFloatCS>();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ClearUAV"),
		ComputeShader,
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp<int32>(PassParameters->NumEntries, 64), 1, 1));
}
