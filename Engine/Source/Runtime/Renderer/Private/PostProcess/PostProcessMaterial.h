// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PostProcess/RenderingCompositionGraph.h"
#include "ScreenPass.h"
#include "OverridePassSequence.h"

class UMaterialInterface;

const uint32 kPostProcessMaterialInputCountMax = 5;

using FPostProcessMaterialChain = TArray<const UMaterialInterface*, TInlineAllocator<10>>;

FPostProcessMaterialChain GetPostProcessMaterialChain(const FViewInfo& View, EBlendableLocation Location);

/** Named post process material slots. Inputs are aliased and have different semantics
 *  based on the post process material blend point, which is documented with the input.
 */
enum class EPostProcessMaterialInput : uint32
{
	// Always Active. Color from the previous stage of the post process chain.
	SceneColor = 0,

	// Always Active.
	SeparateTranslucency = 1,

	// Replace Tonemap Only. Half resolution combined bloom input.
	CombinedBloom = 2,

	// Buffer Visualization Only.
	PreTonemapHDRColor = 2,
	PostTonemapHDRColor = 3,

	// Active if separate velocity pass is used--i.e. not part of base pass; Not active during Replace Tonemap.
	Velocity = 4
};

BEGIN_SHADER_PARAMETER_STRUCT(FPostProcessMaterialParameters, )
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PostProcessOutput)
	SHADER_PARAMETER_STRUCT_ARRAY(FScreenPassTextureInput, PostProcessInput, [kPostProcessMaterialInputCountMax])
	SHADER_PARAMETER_SAMPLER(SamplerState, PostProcessInput_BilinearSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CustomDepth)
	SHADER_PARAMETER(uint32, bFlipYAxis)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

struct FPostProcessMaterialInputs : public FOverridePassInputs
{
	inline void SetInput(EPostProcessMaterialInput Input, FRDGTextureViewRectPair Texture)
	{
		Textures[(uint32)Input] = Texture;
	}

	inline FRDGTextureViewRectPair GetInput(EPostProcessMaterialInput Input) const
	{
		return Textures[(uint32)Input];
	}

	inline void Validate() const
	{
		ValidateInputExists(EPostProcessMaterialInput::SceneColor);
		ValidateInputExists(EPostProcessMaterialInput::SeparateTranslucency);

		// Either override output format is valid or the override output texture is; not both.
		if (OutputFormat != PF_Unknown)
		{
			check(OverrideOutput.Texture == nullptr);
		}
		if (OverrideOutput.Texture)
		{
			check(OutputFormat == PF_Unknown);
		}
	}

	inline void ValidateInputExists(EPostProcessMaterialInput Input) const
	{
		const FRDGTextureViewRectPair Pair = GetInput(EPostProcessMaterialInput::SceneColor);
		check(Pair.Texture);
		check(!Pair.ViewRect.IsEmpty());
	}

	/** Array of input textures bound to the material. The first element represents the output from
	 *  the previous post process and is required. All other inputs are optional.
	 */
	TStaticArray<FRDGTextureViewRectPair, kPostProcessMaterialInputCountMax> Textures;

	/** The output texture format to use if a new texture is created. Uses the input format if left unknown. */
	EPixelFormat OutputFormat = PF_Unknown;

	/** Custom stencil texture used for stencil operations. */
	FRDGTextureRef CustomDepthTexture = nullptr;

	/** Performs a vertical axis flip if the RHI allows it. */
	bool bFlipYAxis = false;
};

using FPostProcessMaterialOutputs = FRDGTextureViewRectPair;

FPostProcessMaterialOutputs AddPostProcessMaterialPass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	const FPostProcessMaterialInputs& Inputs,
	const UMaterialInterface* MaterialInterface);

FPostProcessMaterialOutputs AddPostProcessMaterialChain(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	const FPostProcessMaterialInputs& Inputs,
	const FPostProcessMaterialChain& MaterialChain);

//////////////////////////////////////////////////////////////////////////
//! Legacy Composition Graph Methods

FRenderingCompositePass* AddPostProcessMaterialPass(
	const FPostprocessContext& Context,
	const UMaterialInterface* MaterialInterface,
	EPixelFormat OutputFormat = PF_Unknown);

FRenderingCompositeOutputRef AddPostProcessMaterialChain(
	FPostprocessContext& Context,
	EBlendableLocation InLocation,
	FRenderingCompositeOutputRef SeparateTranslucency = FRenderingCompositeOutputRef(),
	FRenderingCompositeOutputRef PreTonemapHDRColor = FRenderingCompositeOutputRef(),
	FRenderingCompositeOutputRef PostTonemapHDRColor = FRenderingCompositeOutputRef(),
	FRenderingCompositeOutputRef PreFlattenVelocity = FRenderingCompositeOutputRef());

void AddHighResScreenshotMask(FPostprocessContext& Context);