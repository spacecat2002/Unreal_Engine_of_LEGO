// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PostProcessFFTBlooom.cpp: Post processing blom using an FFT-based convolution.
=============================================================================*/

#include "PostProcess/PostProcessFFTBloom.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "GPUFastFourierTransform.h"
#include "ScenePrivate.h"
#include "GlobalShader.h"
#include "Shader.h"
#include "RendererModule.h"

class FFFTShader : public FGlobalShader
{
public:
	// Determine the number of threads used per scanline when writing the physical space kernel
	static const uint32 ThreadsPerGroup = 32;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && !IsMetalMRTPlatform(Parameters.Platform);
	}

	FFFTShader() = default;
	FFFTShader(const CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

class FResizeAndCenterTextureCS : public FFFTShader
{
public:
	DECLARE_GLOBAL_SHADER(FResizeAndCenterTextureCS);
	SHADER_USE_PARAMETER_STRUCT(FResizeAndCenterTextureCS, FFFTShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SrcTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SrcSampler)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DstTexture)
		SHADER_PARAMETER(FIntPoint, DstExtent)
		SHADER_PARAMETER(FIntPoint, ImageExtent)
		SHADER_PARAMETER(FVector4, KernelCenterAndScale)
		SHADER_PARAMETER(FIntPoint, DstBufferExtent)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FFFTShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INCLUDE_RESIZE_AND_CENTER"), 1);
		OutEnvironment.SetDefine(TEXT("THREADS_PER_GROUP"), ThreadsPerGroup);
	}
};

IMPLEMENT_GLOBAL_SHADER(FResizeAndCenterTextureCS, "/Engine/Private/PostProcessFFTBloom.usf", "ResizeAndCenterTextureCS", SF_Compute);

class FCaptureKernelWeightsCS : public FFFTShader
{
public:
	DECLARE_GLOBAL_SHADER(FCaptureKernelWeightsCS);
	SHADER_USE_PARAMETER_STRUCT(FCaptureKernelWeightsCS, FFFTShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HalfResSrcTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, PhysicalSrcTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, PhysicalSrcSampler)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DstTexture)
		SHADER_PARAMETER(FIntPoint, HalfResSumLocation)
		SHADER_PARAMETER(FVector2D, UVCenter)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FFFTShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INCLUDE_CAPTURE_KERNEL_WEIGHTS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCaptureKernelWeightsCS, "/Engine/Private/PostProcessFFTBloom.usf", "CaptureKernelWeightsCS", SF_Compute);

class FBlendLowResCS : public FFFTShader
{
public:
	DECLARE_GLOBAL_SHADER(FBlendLowResCS);
	SHADER_USE_PARAMETER_STRUCT(FBlendLowResCS, FFFTShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SrcTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, HalfResSrcTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HalfResSrcSampler)
		SHADER_PARAMETER_TEXTURE(Texture2D, CenterWeightTexture)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DstTexture)
		SHADER_PARAMETER(FIntRect, DstRect)
		SHADER_PARAMETER(FIntRect, HalfRect)
		SHADER_PARAMETER(FIntPoint, HalfBufferSize)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FFFTShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INCLUDE_BLEND_LOW_RES"), 1);
		OutEnvironment.SetDefine(TEXT("THREADS_PER_GROUP"), ThreadsPerGroup);
	}
};

IMPLEMENT_GLOBAL_SHADER(FBlendLowResCS, "/Engine/Private/PostProcessFFTBloom.usf", "BlendLowResCS", SF_Compute);

class FPassThroughCS : public FFFTShader
{
public:
	DECLARE_GLOBAL_SHADER(FPassThroughCS);
	SHADER_USE_PARAMETER_STRUCT(FPassThroughCS, FFFTShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SrcTexture)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DstTexture)
		SHADER_PARAMETER(FIntRect, SrcRect)
		SHADER_PARAMETER(FIntRect, DstRect)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FFFTShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INCLUDE_PASSTHROUGH"), 1);
		OutEnvironment.SetDefine(TEXT("THREADS_PER_GROUP"), ThreadsPerGroup);
	}
};

IMPLEMENT_GLOBAL_SHADER(FPassThroughCS, "/Engine/Private/PostProcessFFTBloom.usf", "PassThroughCS", SF_Compute);

/**
* Used to resample the physical space kernel into the correct sized buffer with the 
* correct periodicity and center
*
* Resizes the image, moves the center to to 0,0 and applies periodicity 
* across the full TargetSize (periods TargetSize.x & TargetSize.y) 
*
* @param SrcTexture    - SRV for the physical space kernel supplied by user
* @param SrcImageSize  - The extent of the src image
* @param SrcImageCenterUV - The location of the center in src image (e.g. where the kernel center really is).
* @param ResizeScale    - Affective size of the physical space kernel in units of the ImageExtent.x 
* @param TargetSize    - Size of the image produced. 
* @param DstUAV        - Holds the result
* @param DstBufferSize - Size of DstBuffer
* @param bForceCenterZero -  is true only for the experimental 1/2 res version, part of conserving energy
*/
void ResizeAndCenterTexture(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FTextureRHIRef& SrcTexture,
	const FIntPoint& SrcImageSize,
	const FVector2D& SrcImageCenterUV,
	const float ResizeScale,
	const FIntPoint& TargetSize,
	FUnorderedAccessViewRHIRef& DstUAV,
	const FIntPoint& DstBufferSize,
	const bool bForceCenterZero)
{
	SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFTBloom, TEXT("FFT: Pre-process the space kernel to %d by %d"), TargetSize.X, TargetSize.Y);

	// Clamp the image center
	FVector2D ClampedImageCenterUV = SrcImageCenterUV;
	ClampedImageCenterUV.X = FMath::Clamp(SrcImageCenterUV.X, 0.f, 1.f);
	ClampedImageCenterUV.Y = FMath::Clamp(SrcImageCenterUV.Y, 0.f, 1.f);

	check(DstUAV);
	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DstUAV);

	{
		TShaderMapRef<FResizeAndCenterTextureCS> ComputeShader(View.ShaderMap);

		float CenterScale = bForceCenterZero ? 0.f : 1.f;
		const FLinearColor KernelCenterAndScale(ClampedImageCenterUV.X, ClampedImageCenterUV.Y, ResizeScale, CenterScale);

		FResizeAndCenterTextureCS::FParameters PassParameters;
		PassParameters.SrcTexture = SrcTexture;
		PassParameters.SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		PassParameters.DstTexture = DstUAV;
		PassParameters.DstExtent = TargetSize;
		PassParameters.ImageExtent = SrcImageSize;
		PassParameters.KernelCenterAndScale = KernelCenterAndScale;
		PassParameters.DstBufferExtent = DstBufferSize;

		// Use multiple threads per scan line to insure memory coalescing during the write
		const int32 ThreadsPerGroup = FResizeAndCenterTextureCS::ThreadsPerGroup;
		const int32 ThreadsGroupsPerScanLine = (DstBufferSize.X % ThreadsPerGroup == 0) ? DstBufferSize.X / ThreadsPerGroup : DstBufferSize.X / ThreadsPerGroup + 1;
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, FIntVector(ThreadsGroupsPerScanLine, DstBufferSize.Y, 1));
	}
}

/**
* Used by experimental energy conserving 1/2 resolution version of the bloom.
* Captures the sum of the kernel weights represented by the 1/2 res kernel and
* the Center weight from the physical space kernel.
*
* @param Context            - container for RHI and ShaderMap
* @param HalfResKernel      - SRV for the pre-transformed 1/2 res kernel
* @param HalfResSumLocation - The location to sample in the pre-transformed kernel to find the sum of the physical space kernel weights
* @param PhysicalKernel     - SRV for the original physical space kernel
* @param CenterUV           - Where to sample the Physical Kernel for the center weight
* @param CenterWeightRT     - 2x1 float4 buffer that on return will hold result:
*     At  (0,0) the center weight of physical kernel, and (1,0) the sum of the 1/2res kernel weights
*/
void CaptureKernelWeight(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FTextureRHIRef& HalfResKernel,
	const FIntPoint& HalfResSumLocation,
	const FTextureRHIRef& PhysicalKernel,
	const FVector2D& CenterUV,
	TRefCountPtr<IPooledRenderTarget>& CenterWeightRT)
{
	SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFTBloom, TEXT("FFT: Capture Kernel Weights"));

	FSceneRenderTargetItem& DstTargetItem = CenterWeightRT->GetRenderTargetItem();

	check(DstTargetItem.UAV);
	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DstTargetItem.UAV);

	{
		TShaderMapRef<FCaptureKernelWeightsCS> ComputeShader(View.ShaderMap);

		FCaptureKernelWeightsCS::FParameters PassParameters;
		PassParameters.HalfResSrcTexture = HalfResKernel;
		PassParameters.PhysicalSrcTexture = PhysicalKernel;
		PassParameters.PhysicalSrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		PassParameters.DstTexture = DstTargetItem.UAV;
		PassParameters.HalfResSumLocation = HalfResSumLocation;
		PassParameters.UVCenter = CenterUV;

		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, FIntVector(1, 1, 1));
	}

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, DstTargetItem.UAV);

	ensureMsgf(DstTargetItem.TargetableTexture == DstTargetItem.ShaderResourceTexture, TEXT("%s should be resolved to a separate SRV"), *DstTargetItem.TargetableTexture->GetName().ToString());
}

/**
* Used by energy conserving 1/2 resolution version of the bloom.
* This blends the results of the low resolution bloom with the full resolution image 
* in an energy conserving manner.  Assumes the 1/2-res bloom is done with a kernel that
* is missing the center pixel (i.e. the self-gather contribution), and this missing contribution
* is supplied by the full-res image.
*
* @param Context  - container for RHI and ShaderMap
* @param FullResImage - Unbloomed full-resolution source image
* @param FullResImageRect      - Region in FullResImage and DstUAV where the image lives
* @param HaflResConvolvedImage - A 1/2 res image that has been convolved with the bloom kernel minus center.
* @param HalfResRect           - Location of image in the HalfResConvolvedImage buffer
* @param HalfBufferSize        - Full size of the 1/2 Res buffer.
* @param CenterWeightTexture   - Texture that holds the weight between the kernel center and sum of 1/2res kernel weights.
*                                 needed to correctly composite the 1/2 res bloomed result with the full-res image.
* @param DstUAV                - Destination buffer that will hold the result.
*/
void BlendLowRes(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FTextureRHIRef& FullResImage,
	const FIntRect& FullResImageRect,
	const FTextureRHIRef& HalfResConvolvedImage,
	const FIntRect& HalfResRect,
	const FIntPoint& HalfBufferSize,
	const FTextureRHIRef& CenterWeightTexutre,
	FUnorderedAccessViewRHIRef& DstUAV)
{
	SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFTBloom, TEXT("FFT: Post-process upres and blend"));

	// set destination
	check(DstUAV);
	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DstUAV);

	{
		TShaderMapRef<FBlendLowResCS> ComputeShader(View.ShaderMap);

		FBlendLowResCS::FParameters PassParameters;
		PassParameters.SrcTexture = FullResImage;
		PassParameters.DstTexture = DstUAV;
		PassParameters.HalfResSrcTexture = HalfResConvolvedImage;
		PassParameters.HalfResSrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		PassParameters.CenterWeightTexture = CenterWeightTexutre;
		PassParameters.DstRect = FullResImageRect;
		PassParameters.HalfRect = HalfResRect;
		PassParameters.HalfBufferSize = HalfBufferSize;

		const FIntPoint TargetExtent = FullResImageRect.Size();

		// Use multiple threads per scan line to insure memory coalescing during the write
		const int32 ThreadsPerGroup = ComputeShader->ThreadsPerGroup;
		const int32 ThreadsGroupsPerScanLine = (TargetExtent.X % ThreadsPerGroup == 0) ? TargetExtent.X / ThreadsPerGroup : TargetExtent.X / ThreadsPerGroup + 1;

		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, FIntVector(ThreadsGroupsPerScanLine, TargetExtent.Y, 1));
	}

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DstUAV);
}

/**
* Used to copy the input image in the event that it is too large to bloom (i.e. doesn't fit in the FFT group shared memory)
*
* @param SrcTargetItem  - The SrcBuffer to be copied.
* @param SrcRect        - The region in the SrcBuffer to copy
* @param DstUAV         - The target buffer for the copy
* @param DstRect        - The location and region in the target buffer for the copy
*/
void CopyImageRect(FRHICommandList& RHICmdList, const FViewInfo& View, const FSceneRenderTargetItem& SrcTargetItem, const FIntRect& SrcRect, FUnorderedAccessViewRHIRef& DstUAV,  const FIntRect& DstRect)
{
	SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFTBloom, TEXT("FFT: passthrough "));

	// set destination
	check(DstUAV);
	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DstUAV);

	{
		TShaderMapRef<FPassThroughCS> ComputeShader(View.ShaderMap);

		FPassThroughCS::FParameters PassParameters;
		PassParameters.SrcTexture = SrcTargetItem.ShaderResourceTexture;
		PassParameters.DstTexture = DstUAV;
		PassParameters.SrcRect = SrcRect;
		PassParameters.DstRect = DstRect;

		const FIntPoint DstRectSize = DstRect.Size();

		// Use multiple threads per scan line to insure memory coalescing during the write
		const int32 ThreadsPerGroup = ComputeShader->ThreadsPerGroup;
		const int32 ThreadsGroupsPerScanLine = (DstRectSize.X % ThreadsPerGroup == 0) ? DstRectSize.X / ThreadsPerGroup : DstRectSize.X / ThreadsPerGroup + 1;

		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, FIntVector(ThreadsGroupsPerScanLine, DstRectSize.Y, 1));
	}
}

void FRCPassFFTBloom::InitializeDomainParameters(FRenderingCompositePassContext& Context, const float KernelSupportScale, const float KernelSupportScaleClamp)
{
	// We padd by 1/2 the number of pixels the kernel needs in the x-direction
	// so if the kernel is being applied on the edge of the image it will see padding and not periodicity
	// NB:  If the kernel padding would force a transform buffer that is too big for group shared memory (> 4096)
	//      we clamp it.  This could result in a wrap-around in the bloom (from one side of the screen to the other),
	//      but since the amplitude of the bloom kernel tails is usually very small, this shouldn't be too bad.
	auto KernelRadiusSupportFunctor = [KernelSupportScale, KernelSupportScaleClamp](const FIntPoint& Size) ->int32 {

		float ClampedKernelSupportScale = (KernelSupportScaleClamp > 0) ? FMath::Min(KernelSupportScale, KernelSupportScaleClamp) : KernelSupportScale;
		int32 FilterRadius = FMath::CeilToInt(0.5 * ClampedKernelSupportScale * Size.X);
		const int32 MaxFFTSize = GPUFFT::MaxScanLineLength();
		int32 MaxDim = FMath::Max(Size.X, Size.Y);
		if (MaxDim + FilterRadius > MaxFFTSize && MaxDim < MaxFFTSize) FilterRadius = MaxFFTSize - MaxDim;

		return FilterRadius;
	};

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if (!InputDesc)
	{
		// input is not hooked up correctly return dummy kernel
		return;
	}

	InputBufferSize = InputDesc->Extent;

	// Get the source
	TRefCountPtr<IPooledRenderTarget> Input = Context.Pass->GetInput(EPassInputId(0))->GetOutput()->RequestInput();
	InputTargetItem = &Input->GetRenderTargetItem();
	const FTextureRHIRef& InputTexture = InputTargetItem->ShaderResourceTexture;

	// This will be for the actual output.

	OutputTargetItem = const_cast<FSceneRenderTargetItem*>(&PassOutputs[0].RequestSurface(Context));
	FIntPoint OutputBufferSize = PassOutputs[0].RenderTargetDesc.Extent;

	// Determine the region in the source buffer that we want to copy.
	//
	// e.g. 4 means the input texture is 4x smaller than the buffer size
	const uint32 InputScaleFactor  = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, InputBufferSize.Y);
	const uint32 OutputScaleFactor = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, OutputBufferSize.Y);

	const FIntRect InputRect = Context.SceneColorViewRect / InputScaleFactor;
	const FIntRect OutputRect = Context.SceneColorViewRect / OutputScaleFactor;

	// Capture the region of interest
	ImageRect = InputRect;
	const FIntPoint ImageSize = ImageRect.Size();

	// The length of the a side of the square kernel image in pixels

	int32 KernelSize = FMath::CeilToInt(KernelSupportScale * FMath::Max(ImageSize.X, ImageSize.Y));

	int32 SpectralPadding = KernelRadiusSupportFunctor(ImageSize);


	// The following are mathematically equivalent
	// 1) Horizontal FFT / Vertical FFT / Filter / Vertical InvFFT / Horizontal InvFFT
	// 2) Vertical FFT / Horizontal FFT / Filter / Horizontal InvFFT / Vertical InvFFT
	// but we choose the one with the smallest intermediate buffer size

	// The size of the input image plus padding that accounts for
	// the width of the kernel.  The ImageRect is virtually padded
	// with black to account for the gather action of the convolution.
	FIntPoint PaddedImageSize = ImageSize + FIntPoint(SpectralPadding, SpectralPadding);

	PaddedImageSize.X = FMath::Max(PaddedImageSize.X, KernelSize);
	PaddedImageSize.Y = FMath::Max(PaddedImageSize.Y, KernelSize);

	FrequencySize = FIntPoint(FMath::RoundUpToPowerOfTwo(PaddedImageSize.X), FMath::RoundUpToPowerOfTwo(PaddedImageSize.Y));

	// Choose to do to transform in the direction that results in writing the least amount of data to main memory.

	bDoHorizontalFirst = ((FrequencySize.Y * PaddedImageSize.X) > (FrequencySize.X * PaddedImageSize.Y));

	// 
	bIsInitialized = true;
}

bool  FRCPassFFTBloom::TransformKernelFFT(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargetItem& KernelTargetItem)
{
	GPUFFT::FGPUFFTShaderContext FFTContext(RHICmdList, *View.ShaderMap);

	// Create the tmp buffer

	// Our frequency storage layout adds two elements to the first transform direction. 
	const FIntPoint FrequencyPadding = (bDoHorizontalFirst) ? FIntPoint(2, 0) : FIntPoint(0, 2);
	const FIntPoint PaddedFrequencySize = FrequencySize + FrequencyPadding;

	// Should read / write to PF_G16R16F or PF_G32R32F (float2 formats)
	// Need to set the render target description before we "request surface"
	const EPixelFormat PixelFormat = GPUFFT::PixelFormat();
	FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(PaddedFrequencySize, PixelFormat,
		FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

	// Temp buffer used at intermediate buffer when transforming the world space kernel 
	TRefCountPtr<IPooledRenderTarget> TmpRT;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TmpRT, TEXT("FFT Tmp Kernel Buffer"));

	FIntRect SrcRect(FIntPoint(0, 0), FrequencySize);
	const FTextureRHIRef& SrcImage = KernelTargetItem.ShaderResourceTexture;
	FSceneRenderTargetItem& ResultBuffer = KernelTargetItem;

	bool SuccessValue = GPUFFT::FFTImage2D(FFTContext, FrequencySize, bDoHorizontalFirst, SrcRect, SrcImage,  ResultBuffer, TmpRT->GetRenderTargetItem());

	// Transition resource
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ResultBuffer.UAV);
	
	return SuccessValue;
}

bool FRCPassFFTBloom::ConvolveWithKernel(FRHICommandList& RHICmdList, const FViewInfo& View,
	const FTextureRHIRef& SpectralKernelTexture, const FLinearColor& Tint,
	const FTextureRHIRef& SrcTexture, FUnorderedAccessViewRHIRef& ResultUAV,
	const FPreFilter& PreFilter)
{

	if (!bIsInitialized )
	{
		// The dimensions have not be calculated.
		return false;
	}

	GPUFFT::FGPUFFTShaderContext FFTContext(RHICmdList, *View.ShaderMap);

	// Get Tmp buffers required for the Convolution
	
	TRefCountPtr<IPooledRenderTarget> TmpTargets[2];

	const FIntPoint TmpExtent = GPUFFT::Convolution2DBufferSize(FrequencySize, bDoHorizontalFirst, ImageRect.Size());
		//(bDoHorizontalFirst) ? FIntPoint(FrequencySize.X + 2, ImageRect.Size().Y) : FIntPoint(ImageRect.Size().X, FrequencySize.Y + 2);
	FPooledRenderTargetDesc Desc = 
		FPooledRenderTargetDesc::Create2DDesc(TmpExtent, GPUFFT::PixelFormat(), FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TmpTargets[0], TEXT("Tmp FFT Buffer A"));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TmpTargets[1], TEXT("Tmp FFT Buffer B"));
	
	// Get the source
	const FTextureRHIRef& InputTexture = SrcTexture;

	bool SuccessValue =
		GPUFFT::ConvolutionWithTextureImage2D(FFTContext, FrequencySize, bDoHorizontalFirst,
				SpectralKernelTexture,
			    ImageRect/*region of interest*/,
				InputTexture, 
				ResultUAV,
				TmpTargets[0]->GetRenderTargetItem(),
				TmpTargets[1]->GetRenderTargetItem(),
				PreFilter);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, ResultUAV);

	return SuccessValue;
}

FSceneRenderTargetItem* FRCPassFFTBloom::InitDomainAndGetKernel(FRenderingCompositePassContext& Context)
{
	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	const auto& PPSettings = View.FinalPostProcessSettings;

	// The kernel parameters on the FinalPostProcess.  

	UTexture2D* BloomConvolutionTexture = PPSettings.BloomConvolutionTexture;

	if (BloomConvolutionTexture == nullptr)
	{
		BloomConvolutionTexture = GEngine->DefaultBloomKernelTexture;
	}

	const float BloomConvolutionSize    = PPSettings.BloomConvolutionSize;
	const FVector2D CenterUV            = PPSettings.BloomConvolutionCenterUV;
	const float ClampedBloomConvolutionBufferScale = FMath::Clamp(PPSettings.BloomConvolutionBufferScale, 0.f, 1.f);
	

	// The pre-filter boost parameters for bright pixels. Because the Convolution PP work in pre-exposure space, the min and max needs adjustment.
	const FVector PreFilter(PPSettings.BloomConvolutionPreFilterMin * View.PreExposure, PPSettings.BloomConvolutionPreFilterMax * View.PreExposure, PPSettings.BloomConvolutionPreFilterMult);

	// Clip the Kernel support (i.e. bloom size) to 100% the screen width 
	const float MaxBloomSize = 1.f;
	const float ClampedBloomSizeScale = FMath::Clamp(BloomConvolutionSize, 0.f, MaxBloomSize);

	// Set up the buffer sizes
	InitializeDomainParameters(Context, ClampedBloomSizeScale, ClampedBloomConvolutionBufferScale);

	if (bIsInitialized == false) return nullptr;

	// The transform kernel gets cached in the view state.
	if (!ViewState)
	{
		// input is not hooked up correctly, return a null pointer.
		return nullptr;
	}
	// redundant check
	if (!BloomConvolutionTexture || !BloomConvolutionTexture->Resource)  return nullptr;

	// Our frequency storage layout adds two elements to the first transform direction. 
	const FIntPoint FrequencyPadding = (bDoHorizontalFirst) ? FIntPoint(2, 0) : FIntPoint(0, 2);
	const FIntPoint PaddedFrequencySize = FrequencySize + FrequencyPadding;

	// Should read / write to PF_G16R16F or PF_G32R32F (float2 formats)
	// Need to set the render target description before we "request surface"
	const EPixelFormat PixelFormat = GPUFFT::PixelFormat();
	const FPooledRenderTargetDesc TransformDesc = FPooledRenderTargetDesc::Create2DDesc(PaddedFrequencySize, PixelFormat,
		FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

	auto& FFTKernel = ViewState->BloomFFTKernel;
	// Get the FFT kernel from the view state (note, this has already been transformed).
	TRefCountPtr<IPooledRenderTarget>& TransformedKernelRT = FFTKernel.Spectral;
	const UTexture2D* CachedKernelPhysical                 = FFTKernel.Physical;
	const float       CachedKernelScale                    = FFTKernel.Scale;
	const FVector2D&  CachedKernelCenterUV                 = FFTKernel.CenterUV;
	const FIntPoint&  CachedImageSize                      = FFTKernel.ImageSize;

	const FIntPoint ImageSize = ImageRect.Size();
	// Check if the FFT kernel is dirty 

	bool bCachedKernelIsDirty = true;
	if (TransformedKernelRT)
	{
		FPooledRenderTarget* TransformedTexture = (FPooledRenderTarget*)TransformedKernelRT.GetReference();

		const bool bSameTexture        = (CachedKernelPhysical == static_cast<const UTexture2D*>(BloomConvolutionTexture));
		const bool bSameSpectralBuffer = TransformedTexture->GetDesc().Compare(TransformDesc, true /*exact match*/);
		const bool bSameKernelSize     = FMath::IsNearlyEqual(CachedKernelScale, BloomConvolutionSize, float(1.e-6) /*tol*/);
		const bool bSameImageSize      = (ImageSize == CachedImageSize);
		const bool bSameKernelCenterUV = CachedKernelCenterUV.Equals(CenterUV, float(1.e-6) /*tol*/);
		const bool bSameMipLevel       = bSameTexture && (
			FFTKernel.PhysicalMipLevel == static_cast<FTexture2DResource*>(BloomConvolutionTexture->Resource)->GetCurrentFirstMip());

		if (bSameTexture && bSameSpectralBuffer && bSameKernelSize && bSameImageSize && bSameKernelCenterUV && bSameMipLevel)
		{
			bCachedKernelIsDirty = false;
		}
	}

	const bool bIsHalfResolutionFFT = bHalfResolutionFFT();

	// Re-transform the kernel if needed.
	if (bCachedKernelIsDirty)
	{
		// Resize the buffer to hold the transformed kernel
		GRenderTargetPool.FindFreeElement(Context.RHICmdList, TransformDesc, TransformedKernelRT, TEXT("FFTKernel"));

		// NB: SpectralKernelRTItem is member data
		FSceneRenderTargetItem& SpectralKernelRTItem = TransformedKernelRT->GetRenderTargetItem();
		FUnorderedAccessViewRHIRef SpectralKernelUAV = SpectralKernelRTItem.UAV;

		// Sample the physical space kernel into the resized buffer

		FTextureRHIRef& PhysicalSpaceKernelTextureRef = BloomConvolutionTexture->Resource->TextureRHI;

		// Rescale the physical space kernel ( and omit the center if this is a 1/2 resolution fft, it will be added later)

		ResizeAndCenterTexture(Context.RHICmdList, Context.View, PhysicalSpaceKernelTextureRef, ImageSize, CenterUV, ClampedBloomSizeScale,
			FrequencySize, SpectralKernelRTItem.UAV, PaddedFrequencySize, bIsHalfResolutionFFT);

		Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, SpectralKernelRTItem.UAV);

		// Two Dimensional FFT of the physical space kernel.  
		// Input: SpectralRTItem holds the physical space kernel, on return it will be the spectral space 

		TransformKernelFFT(Context.RHICmdList, Context.View, SpectralKernelRTItem);

		if (bIsHalfResolutionFFT)
		{
			TRefCountPtr<IPooledRenderTarget>& CenterWeightRT = FFTKernel.CenterWeight;

			const FPooledRenderTargetDesc CenterWeightDesc = FPooledRenderTargetDesc::Create2DDesc(FIntPoint(2, 1), PixelFormat,
				FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

			// Resize the buffer to hold the transformed kernel
			GRenderTargetPool.FindFreeElement(Context.RHICmdList, CenterWeightDesc, CenterWeightRT, TEXT("FFTKernelCenterWeight"));

			const FTextureRHIRef& HalfResKernelTextureRef = SpectralKernelRTItem.ShaderResourceTexture;

			const FIntPoint& HalfResKernelExtent = PaddedFrequencySize;

			const FIntPoint HalfResSumLocation = (bDoHorizontalFirst) ? FIntPoint(HalfResKernelExtent.X, 0) : FIntPoint(0, HalfResKernelExtent.Y);
			// Capture the missing center weight from the kernel and the sum of the existing weights.
			CaptureKernelWeight(Context.RHICmdList, Context.View, HalfResKernelTextureRef, HalfResKernelExtent, PhysicalSpaceKernelTextureRef, CenterUV, CenterWeightRT);
		}

		// Update the data on the ViewState
		ViewState->BloomFFTKernel.Scale = BloomConvolutionSize;
		ViewState->BloomFFTKernel.ImageSize = ImageSize;
		ViewState->BloomFFTKernel.Physical = BloomConvolutionTexture;
		ViewState->BloomFFTKernel.CenterUV = CenterUV;
		ViewState->BloomFFTKernel.PhysicalMipLevel = static_cast<FTexture2DResource*>(BloomConvolutionTexture->Resource)->GetCurrentFirstMip();
	}

	// Return pointer to the transformed kernel.
	return &(TransformedKernelRT->GetRenderTargetItem());
}

bool FRCPassFFTBloom::ConvolveImageWithKernel(FRenderingCompositePassContext& Context)
{
	// Init the domain data update the cached kernel if needed.
	FSceneRenderTargetItem* SpectralKernelRTItem = InitDomainAndGetKernel(Context);

	// was the domain too large?  did something else fail?
	if (!SpectralKernelRTItem) return false;

	// Do the convolution with the kernel

	const FTextureRHIRef& SpectralKernelTexture = SpectralKernelRTItem->ShaderResourceTexture;

	const bool bIsHalfResolutionFFT = bHalfResolutionFFT();

	const FViewInfo& View = Context.View;
	const auto& FinalPPSettings = View.FinalPostProcessSettings;
	// The pre-filter boost parameters for bright pixels. Because the Convolution PP work in pre-exposure space, the min and max needs adjustment.
	const FVector PreFilter(FinalPPSettings.BloomConvolutionPreFilterMin * View.PreExposure, FinalPPSettings.BloomConvolutionPreFilterMax * View.PreExposure, FinalPPSettings.BloomConvolutionPreFilterMult);

	const FLinearColor Tint(1, 1, 1, 1);

	if (bIsHalfResolutionFFT)
	{

		// Get a half-resolution destination buffer.
		TRefCountPtr<IPooledRenderTarget> HalfResConvolutionResult;
		
		const EPixelFormat PixelFormat = GPUFFT::PixelFormat();

		const FPooledRenderTargetDesc HalfResFFTDesc = FPooledRenderTargetDesc::Create2DDesc(InputBufferSize, PixelFormat,
			FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

		GRenderTargetPool.FindFreeElement(Context.RHICmdList, HalfResFFTDesc, HalfResConvolutionResult, TEXT("HalfRes FFT Result"));
		FSceneRenderTargetItem& HalfResConvolutionRTItem = HalfResConvolutionResult->GetRenderTargetItem();

		// The FFT result buffer is also half res.
	
		ConvolveWithKernel(Context.RHICmdList, Context.View, SpectralKernelTexture, Tint, InputTargetItem->ShaderResourceTexture , HalfResConvolutionRTItem.UAV, PreFilter);

		// The blend weighting parameters from the View State

		FSceneViewState* ViewState = (FSceneViewState*)View.State;
		auto& FFTKernel = ViewState->BloomFFTKernel;

		const FTextureRHIRef& CenterWeightTexture = FFTKernel.CenterWeight->GetRenderTargetItem().ShaderResourceTexture;

		// The output buffer
		// NB: the Target buffer and source buffer have the same extent.

		FSceneRenderTargetItem& PassOutput = *OutputTargetItem;

		// Get full resolution source

		const TRefCountPtr<IPooledRenderTarget>& FullResRT = Context.Pass->GetInput(ePId_Input1)->GetOutput()->RequestInput();
		const FTextureRHIRef& FullResResourceTexture = FullResRT->GetRenderTargetItem().ShaderResourceTexture;

		// Blend with  alpha * SrcBuffer + betta * BloomedBuffer  where alpha = Weights[0], beta = Weights[1]
		const FIntPoint& HalfResBufferSize = InputBufferSize;
		BlendLowRes(Context.RHICmdList, Context.View, FullResResourceTexture, Context.SceneColorViewRect, HalfResConvolutionRTItem.ShaderResourceTexture, ImageRect, HalfResBufferSize, CenterWeightTexture, PassOutput.UAV);
	}
	else
	{
		// Do Convolution directly into the output buffer
		// NB: In this case there is only one input, and the output has matching resolution
		ConvolveWithKernel(Context.RHICmdList, Context.View, SpectralKernelTexture, Tint, InputTargetItem->ShaderResourceTexture, OutputTargetItem->UAV, PreFilter);
	}

	return true;
}

void FRCPassFFTBloom::PassThroughImage(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	// Copy the Image content and location
	const FIntRect& InputRect  = ImageRect;
	const FIntRect& OutputRect = ImageRect;
	CopyImageRect(RHICmdList, View, *InputTargetItem, InputRect, OutputTargetItem->UAV, OutputRect);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutputTargetItem->UAV);
}

void FRCPassFFTBloom::Process(FRenderingCompositePassContext& Context)
{
	bool bSucesss = ConvolveImageWithKernel(Context);

	// Fail gracefully by just copying the input image without convolution.
	// Currently this will happen if the transform lengths are too large
	// for group shared memory or if the Context.View.State is invalid.
	if (!bSucesss)
	{
		PassThroughImage(Context.RHICmdList, Context.View);
	}
}

FPooledRenderTargetDesc FRCPassFFTBloom::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	//  The optional second input will override the output format and size
	const bool bIsHalfResolutionFFT = bHalfResolutionFFT();

	EPassInputId PassInputId = (bIsHalfResolutionFFT) ? ePId_Input1 : ePId_Input0;

	const FPooledRenderTargetDesc& SrcRet = GetInput(PassInputId)->GetOutput()->RenderTargetDesc;

	FIntPoint Extent = SrcRet.Extent;
	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(SrcRet.Extent, SrcRet.Format, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));

	Ret.DebugName = TEXT("FFTBuffer");

	return Ret;
}


bool FRCPassFFTBloom::HasValidPhysicalKernel(FPostprocessContext& Context)
{
	const FViewInfo& View = Context.View;

	UTexture2D* BloomConvolutionTexture = View.FinalPostProcessSettings.BloomConvolutionTexture;

	// Fall back to the default bloom texture if provided.
	if (BloomConvolutionTexture == nullptr)
	{
		BloomConvolutionTexture = GEngine->DefaultBloomKernelTexture;
	}

	bool bValidSetup = (BloomConvolutionTexture != nullptr && BloomConvolutionTexture->Resource != nullptr);

	// The bloom convolution kernel needs to be resident to avoid visual artifacts.
	if (bValidSetup)
	{
		const int32 CinematicTextureGroups = 0;
		const float Seconds = 5.0f;
		BloomConvolutionTexture->SetForceMipLevelsToBeResident(Seconds, CinematicTextureGroups);
	}

	const uint32 FramesPerWarning = 15;

	if (bValidSetup && BloomConvolutionTexture->IsFullyStreamedIn() == false)
	{
		if ((View.Family->FrameNumber % FramesPerWarning) == 0)
		{
			UE_LOG(LogRenderer, Warning, TEXT("The Physical Kernel Texture not fully streamed in."));
		}
	}

	bValidSetup = bValidSetup && (BloomConvolutionTexture->IsFullyStreamedIn() == true);

	if (bValidSetup && BloomConvolutionTexture->bHasStreamingUpdatePending == true)
	{
		if ((View.Family->FrameNumber % FramesPerWarning) == 0)
		{
			UE_LOG(LogRenderer, Warning, TEXT("The Physical Kernel Texture has pending update."));
		}
	}

	bValidSetup = bValidSetup && (BloomConvolutionTexture->bHasStreamingUpdatePending == false);

	return bValidSetup;
}
