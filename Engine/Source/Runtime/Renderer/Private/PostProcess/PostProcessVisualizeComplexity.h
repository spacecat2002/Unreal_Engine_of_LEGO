// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "DebugViewModeHelpers.h"
#include "OverridePassSequence.h"

/**
* The number of shader complexity colors from the engine ini that will be passed to the shader. 
* Changing this requires a recompile of the FShaderComplexityApplyPS.
*/
const uint32 MaxNumShaderComplexityColors = 11;
const float NormalizedQuadComplexityValue = 1.f / 16.f;

// Gets the maximum shader complexity count from the ini settings.
float GetMaxShaderComplexityCount(ERHIFeatureLevel::Type InFeatureType);

struct FVisualizeComplexityInputs : public FOverridePassInputs
{
	enum class EColorSamplingMethod : uint32
	{
		Ramp,
		Linear,
		Stair
	};

	// The input scene color and view rect.
	FRDGTextureViewRectPair SceneColor;

	// The table of colors used for visualization, ordered by least to most complex.
	TArrayView<const FLinearColor> Colors;

	// The method used to derive a color from the sampled complexity value.
	EColorSamplingMethod ColorSamplingMethod = EColorSamplingMethod::Ramp;

	// A scale applied to the sampled scene complexity value prior to colorizing.
	float ComplexityScale = 1.0f;

	// Renders the complexity legend overlay.
	bool bDrawLegend = false;
};

using FVisualizeComplexityOutputs = FRDGTextureViewRectPair;

FVisualizeComplexityOutputs AddVisualizeComplexityPass(FRDGBuilder& GraphBuilder, const FScreenPassViewInfo& ScreenPassView, const FVisualizeComplexityInputs& Inputs);

FRenderingCompositeOutputRef AddVisualizeComplexityPass(
	FRenderingCompositionGraph& Graph,
	FRenderingCompositeOutputRef Input,
	const TArray<FLinearColor>& InColors,
	FVisualizeComplexityInputs::EColorSamplingMethod InColorSampling,
	float InComplexityScale,
	bool bInLegend);