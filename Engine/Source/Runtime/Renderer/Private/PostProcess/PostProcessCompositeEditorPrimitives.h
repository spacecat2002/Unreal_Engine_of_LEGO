// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"
#include "PostProcess/RenderingCompositionGraph.h"

#if WITH_EDITOR

/** Base class for a global pixel shader which renders editor primitives (outlines, helpers, etc). */
class FEditorPrimitiveShader : public FGlobalShader
{
public:
	static const uint32 kEditorMSAASampleCountMax = 8;

	class FSampleCountDimension : SHADER_PERMUTATION_RANGE_INT("MSAA_SAMPLE_COUNT", 1, kEditorMSAASampleCountMax + 1);
	using FPermutationDomain = TShaderPermutationDomain<FSampleCountDimension>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 SampleCount = PermutationVector.Get<FSampleCountDimension>();

		// Only SM5+ platforms supports MSAA.
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && SampleCount > 1)
		{
			return false;
		}

		// Only use permutations with valid MSAA sample counts.
		if (!FMath::IsPowerOfTwo(SampleCount))
		{
			return false;
		}

		// Only PC platforms render editor primitives.
		return IsPCPlatform(Parameters.Platform);
	}

	FEditorPrimitiveShader() = default;
	FEditorPrimitiveShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

// Constructs a new view suitable for rendering editor primitives (outlines, helpers, etc).
const FViewInfo* UpdateEditorPrimitiveView(
	FPersistentUniformBuffers& SceneUniformBuffers,
	FSceneRenderTargets& SceneContext,
	const FViewInfo& ParentView,
	FIntRect ViewRect);

struct FEditorPrimitiveInputs : public FOverridePassInputs
{
	enum class EBasePassType : uint32
	{
		Deferred,
		Mobile,
		MAX
	};

	FScreenPassTexture SceneColor;
	FScreenPassTexture SceneDepth;

	EBasePassType BasePassType = EBasePassType::MAX;
};

using FEditorPrimitiveOutputs = FScreenPassTexture;

FEditorPrimitiveOutputs AddEditorPrimitivePass(FRDGBuilder& GraphBuilder, const FScreenPassViewInfo& ScreenPassView, const FEditorPrimitiveInputs& Inputs);

FRenderingCompositeOutputRef AddEditorPrimitivePass(
	FRenderingCompositionGraph& Graph,
	FRenderingCompositeOutputRef Input,
	FEditorPrimitiveInputs::EBasePassType BasePassType);

#endif
