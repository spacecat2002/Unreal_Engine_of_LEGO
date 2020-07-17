// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLES.cpp: OpenGL ES implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"

#if !PLATFORM_DESKTOP

#if OPENGL_ES

PFNEGLGETSYSTEMTIMENVPROC eglGetSystemTimeNV_p = NULL;
PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_p = NULL;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_p = NULL;
PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_p = NULL;
PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_p = NULL;

namespace GLFuncPointers
{
	// Offscreen MSAA rendering
	PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC	glFramebufferTexture2DMultisampleEXT = NULL;
	PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC	glRenderbufferStorageMultisampleEXT = NULL;

	PFNGLPUSHGROUPMARKEREXTPROC				glPushGroupMarkerEXT = NULL;
	PFNGLPOPGROUPMARKEREXTPROC				glPopGroupMarkerEXT = NULL;
	PFNGLLABELOBJECTEXTPROC					glLabelObjectEXT = NULL;
	PFNGLGETOBJECTLABELEXTPROC				glGetObjectLabelEXT = NULL;

	// KHR_debug
	PFNGLDEBUGMESSAGECONTROLKHRPROC			glDebugMessageControlKHR = NULL;
	PFNGLDEBUGMESSAGEINSERTKHRPROC			glDebugMessageInsertKHR = NULL;
	PFNGLDEBUGMESSAGECALLBACKKHRPROC		glDebugMessageCallbackKHR = NULL;
	PFNGLGETDEBUGMESSAGELOGKHRPROC			glDebugMessageLogKHR = NULL;
	PFNGLGETPOINTERVKHRPROC					glGetPointervKHR = NULL;
	PFNGLPUSHDEBUGGROUPKHRPROC				glPushDebugGroupKHR = NULL;
	PFNGLPOPDEBUGGROUPKHRPROC				glPopDebugGroupKHR = NULL;
	PFNGLOBJECTLABELKHRPROC					glObjectLabelKHR = NULL;
	PFNGLGETOBJECTLABELKHRPROC				glGetObjectLabelKHR = NULL;
	PFNGLOBJECTPTRLABELKHRPROC				glObjectPtrLabelKHR = NULL;
	PFNGLGETOBJECTPTRLABELKHRPROC			glGetObjectPtrLabelKHR = NULL;

	// ES 3.2
	PFNGLTEXBUFFEREXTPROC					glTexBufferEXT = nullptr;
	PFNGLTEXBUFFERRANGEEXTPROC				glTexBufferRangeEXT = nullptr;
	PFNGLCOPYIMAGESUBDATAEXTPROC			glCopyImageSubDataEXT = nullptr;
	PFNGLENABLEIEXTPROC						glEnableiEXT = nullptr;
	PFNGLDISABLEIEXTPROC					glDisableiEXT = nullptr;
	PFNGLBLENDEQUATIONIEXTPROC				glBlendEquationiEXT = nullptr;
	PFNGLBLENDEQUATIONSEPARATEIEXTPROC		glBlendEquationSeparateiEXT = nullptr;
	PFNGLBLENDFUNCIEXTPROC					glBlendFunciEXT = nullptr;
	PFNGLBLENDFUNCSEPARATEIEXTPROC			glBlendFuncSeparateiEXT = nullptr;
	PFNGLCOLORMASKIEXTPROC					glColorMaskiEXT = nullptr;

	PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC				glFramebufferTextureMultiviewOVR = NULL;
	PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC	glFramebufferTextureMultisampleMultiviewOVR = NULL;
};

/** GL_EXT_disjoint_timer_query */
bool FOpenGLES::bSupportsDisjointTimeQueries = false;

static TAutoConsoleVariable<int32> CVarDisjointTimerQueries(
	TEXT("r.DisjointTimerQueries"),
	0,
	TEXT("If set to 1, allows GPU time to be measured (e.g. STAT UNIT). It defaults to 0 because some devices supports it but very slowly."),
	ECVF_ReadOnly);

/** Some timer query implementations are never disjoint */
bool FOpenGLES::bTimerQueryCanBeDisjoint = true;

/** GL_APPLE_texture_format_BGRA8888 */
bool FOpenGLES::bSupportsBGRA8888 = false;

/** Whether BGRA supported as color attachment */
bool FOpenGLES::bSupportsBGRA8888RenderTarget = false;

/** GL_OES_texture_float */
bool FOpenGLES::bSupportsTextureFloat = false;

/** GL_OES_texture_half_float */
bool FOpenGLES::bSupportsTextureHalfFloat = false;

/** GL_EXT_color_buffer_half_float */
bool FOpenGLES::bSupportsColorBufferHalfFloat = false;

/** GL_EXT_color_buffer_float */
bool FOpenGLES::bSupportsColorBufferFloat = false;

/** GL_EXT_shader_framebuffer_fetch */
bool FOpenGLES::bSupportsShaderFramebufferFetch = false;

/* This is to avoid a bug where device supports GL_EXT_shader_framebuffer_fetch but does not define it in GLSL */
bool FOpenGLES::bRequiresUEShaderFramebufferFetchDef = false;

/** GL_ARM_shader_framebuffer_fetch_depth_stencil */
bool FOpenGLES::bSupportsShaderDepthStencilFetch = false;

/** GL_EXT_multisampled_render_to_texture */
bool FOpenGLES::bSupportsMultisampledRenderToTexture = false;

/** GL_NV_texture_compression_s3tc, GL_EXT_texture_compression_s3tc */
bool FOpenGLES::bSupportsDXT = false;

/** OpenGL ES 3.0 profile */
bool FOpenGLES::bSupportsETC2 = false;

/** GL_FRAGMENT_SHADER, GL_LOW_FLOAT */
int FOpenGLES::ShaderLowPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT */
int FOpenGLES::ShaderMediumPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_HIGH_FLOAT */
int FOpenGLES::ShaderHighPrecision = 0;

/** GL_NV_framebuffer_blit */
bool FOpenGLES::bSupportsNVFrameBufferBlit = false;

/* This is a hack to remove the gl_FragCoord if shader will fail to link if exceeding the max varying on android platforms */
bool FOpenGLES::bRequiresGLFragCoordVaryingLimitHack = false;

/* This indicates failure when attempting to retrieve driver's binary representation of the hack program  */
bool FOpenGLES::bBinaryProgramRetrievalFailed = false;

/* This is a hack to add a round() function when not available to a shader compiler */
bool FOpenGLES::bRequiresRoundFunctionHack = true;

/* Some Mali devices do not work correctly with early_fragment_test enabled */
bool FOpenGLES::bRequiresDisabledEarlyFragmentTests = false;

/* This is to avoid a bug in Adreno drivers that define GL_ARM_shader_framebuffer_fetch_depth_stencil even when device does not support this extension  */
bool FOpenGLES::bRequiresARMShaderFramebufferFetchDepthStencilUndef = false;

/* Indicates shader compiler hack checks are being tested */
bool FOpenGLES::bIsCheckingShaderCompilerHacks = false;

/* Indicates shader compiler hack checks are being tested */
bool FOpenGLES::bIsLimitingShaderCompileCount = false;

bool FOpenGLES::bUseHalfFloatTexStorage = false;
bool FOpenGLES::bUseES30ShadingLanguage = false;
bool FOpenGLES::bES31Support = false;
bool FOpenGLES::bHasHardwareHiddenSurfaceRemoval = false;
bool FOpenGLES::bSupportsMobileMultiView = false;
GLint FOpenGLES::MaxMSAASamplesTileMem = 1;

GLint FOpenGLES::MaxComputeTextureImageUnits = -1;
GLint FOpenGLES::MaxComputeUniformComponents = -1;

GLint FOpenGLES::MaxComputeUAVUnits = -1;
GLint FOpenGLES::MaxPixelUAVUnits = -1;
GLint FOpenGLES::MaxCombinedUAVUnits = 0;

FOpenGLES::EFeatureLevelSupport FOpenGLES::CurrentFeatureLevelSupport = FOpenGLES::EFeatureLevelSupport::ES31;

bool FOpenGLES::SupportsDisjointTimeQueries()
{
	bool bAllowDisjointTimerQueries = false;
	bAllowDisjointTimerQueries = (CVarDisjointTimerQueries.GetValueOnRenderThread() == 1);
	return bSupportsDisjointTimeQueries && bAllowDisjointTimerQueries;
}

void FOpenGLES::ProcessQueryGLInt()
{
	GLint MaxVertexAttribs;
	LOG_AND_GET_GL_INT(GL_MAX_VERTEX_ATTRIBS, 0, MaxVertexAttribs);
	if (MaxVertexAttribs < 16)
	{
		UE_LOG(LogRHI, Error,
			TEXT("Device reports support for %d vertex attributes, UE4 requires 16. Rendering artifacts may occur."),
			MaxVertexAttribs
		);
	}

	LOG_AND_GET_GL_INT(GL_MAX_VARYING_VECTORS, 0, MaxVaryingVectors);
	LOG_AND_GET_GL_INT(GL_MAX_VERTEX_UNIFORM_VECTORS, 0, MaxVertexUniformComponents);
	LOG_AND_GET_GL_INT(GL_MAX_FRAGMENT_UNIFORM_VECTORS, 0, MaxPixelUniformComponents);
	LOG_AND_GET_GL_INT(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, 0, TextureBufferAlignment);

	const GLint RequiredMaxVertexUniformComponents = 256;
	if (MaxVertexUniformComponents < RequiredMaxVertexUniformComponents)
	{
		UE_LOG(LogRHI, Warning,
			TEXT("Device reports support for %d vertex uniform vectors, UE4 requires %d. Rendering artifacts may occur, especially with skeletal meshes. Some drivers, e.g. iOS, report a smaller number than is actually supported."),
			MaxVertexUniformComponents,
			RequiredMaxVertexUniformComponents
		);
	}
	MaxVertexUniformComponents = FMath::Max<GLint>(MaxVertexUniformComponents, RequiredMaxVertexUniformComponents);

	MaxGeometryUniformComponents = 0;

	MaxGeometryTextureImageUnits = 0;
	MaxHullTextureImageUnits = 0;
	MaxDomainTextureImageUnits = 0;
}

void FOpenGLES::ProcessExtensions(const FString& ExtensionsString)
{
	ProcessQueryGLInt();
	FOpenGLBase::ProcessExtensions(ExtensionsString);

	bSupportsDisjointTimeQueries = ExtensionsString.Contains(TEXT("GL_EXT_disjoint_timer_query")) || ExtensionsString.Contains(TEXT("GL_NV_timer_query"));
	bTimerQueryCanBeDisjoint = !ExtensionsString.Contains(TEXT("GL_NV_timer_query"));
	bSupportsBGRA8888 = ExtensionsString.Contains(TEXT("GL_APPLE_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_IMG_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_format_BGRA8888"));
	bSupportsBGRA8888RenderTarget = bSupportsBGRA8888;
	bSupportsTextureFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_float"));
	bSupportsTextureHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_half_float"));
	bSupportsColorBufferFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_float"));
	bSupportsColorBufferHalfFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
	bSupportsShaderFramebufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch"))
		|| ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch ")); // has space at the end to exclude GL_ARM_shader_framebuffer_fetch_depth_stencil match
	bRequiresUEShaderFramebufferFetchDef = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch"));
	bSupportsShaderDepthStencilFetch = ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch_depth_stencil"));
	bSupportsMultisampledRenderToTexture = ExtensionsString.Contains(TEXT("GL_EXT_multisampled_render_to_texture"));
	bSupportsDXT = ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc"));
	bSupportsNVFrameBufferBlit = ExtensionsString.Contains(TEXT("GL_NV_framebuffer_blit"));

	// Report shader precision
	int Range[2];
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, Range, &ShaderLowPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, Range, &ShaderMediumPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, Range, &ShaderHighPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader lowp precision: %d"), ShaderLowPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader mediump precision: %d"), ShaderMediumPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader highp precision: %d"), ShaderHighPrecision);

	if (FPlatformMisc::IsDebuggerPresent() && UE_BUILD_DEBUG)
	{
		// Enable GL debug markers if we're running in Xcode
		extern int32 GEmitMeshDrawEvent;
		GEmitMeshDrawEvent = 1;
		SetEmitDrawEvents(true);
	}

	GSupportsDepthRenderTargetWithoutColorRenderTarget = false;

	glPushGroupMarkerEXT = (PFNGLPUSHGROUPMARKEREXTPROC)((void*)eglGetProcAddress("glPushGroupMarkerEXT"));
	glPopGroupMarkerEXT = (PFNGLPOPGROUPMARKEREXTPROC)((void*)eglGetProcAddress("glPopGroupMarkerEXT"));

	if (ExtensionsString.Contains(TEXT("GL_EXT_DEBUG_LABEL")))
	{
		glLabelObjectEXT = (PFNGLLABELOBJECTEXTPROC)((void*)eglGetProcAddress("glLabelObjectEXT"));
		glGetObjectLabelEXT = (PFNGLGETOBJECTLABELEXTPROC)((void*)eglGetProcAddress("glGetObjectLabelEXT"));
	}

	if (ExtensionsString.Contains(TEXT("GL_EXT_multisampled_render_to_texture")))
	{
		glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)((void*)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT"));
		glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)((void*)eglGetProcAddress("glRenderbufferStorageMultisampleEXT"));
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &MaxMSAASamplesTileMem);
		MaxMSAASamplesTileMem = FMath::Max<GLint>(MaxMSAASamplesTileMem, 1);
		UE_LOG(LogRHI, Log, TEXT("Support for %dx MSAA detected"), MaxMSAASamplesTileMem);
	}
	else
	{
		// indicates RHI supports on-chip MSAA but this device does not.
		MaxMSAASamplesTileMem = 1;
	}

	GET_GL_INT(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, 0, MaxComputeTextureImageUnits);
	GET_GL_INT(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, 0, MaxComputeUniformComponents);

	LOG_AND_GET_GL_INT(GL_MAX_COMBINED_IMAGE_UNIFORMS, 0, MaxCombinedUAVUnits);
	LOG_AND_GET_GL_INT(GL_MAX_COMPUTE_IMAGE_UNIFORMS, 0, MaxComputeUAVUnits);
	LOG_AND_GET_GL_INT(GL_MAX_FRAGMENT_IMAGE_UNIFORMS, 0, MaxPixelUAVUnits);

	// clamp UAV units to a sensible limit
	MaxCombinedUAVUnits = FMath::Min(MaxCombinedUAVUnits, 8);
	MaxComputeUAVUnits = FMath::Min(MaxComputeUAVUnits, MaxCombinedUAVUnits);
	MaxPixelUAVUnits = FMath::Min(MaxPixelUAVUnits, MaxCombinedUAVUnits);

	bSupportsETC2 = true;
	bUseES30ShadingLanguage = true;
	
	// Required by the ES3 spec
	bSupportsTextureFloat = true;
	bSupportsTextureHalfFloat = true;

	// According to https://www.khronos.org/registry/gles/extensions/EXT/EXT_color_buffer_float.txt
	bSupportsColorBufferHalfFloat = (bSupportsColorBufferHalfFloat || bSupportsColorBufferFloat);

	GSupportsDepthRenderTargetWithoutColorRenderTarget = true;
	
	// Mobile multi-view setup
	const bool bMultiViewSupport = ExtensionsString.Contains(TEXT("GL_OVR_multiview"));
	const bool bMultiView2Support = ExtensionsString.Contains(TEXT("GL_OVR_multiview2"));
	const bool bMultiViewMultiSampleSupport = ExtensionsString.Contains(TEXT("GL_OVR_multiview_multisampled_render_to_texture"));
	if (bMultiViewSupport && bMultiView2Support && bMultiViewMultiSampleSupport)
	{
		glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)((void*)eglGetProcAddress("glFramebufferTextureMultiviewOVR"));
		glFramebufferTextureMultisampleMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)((void*)eglGetProcAddress("glFramebufferTextureMultisampleMultiviewOVR"));

		bSupportsMobileMultiView = (glFramebufferTextureMultiviewOVR != NULL) && (glFramebufferTextureMultisampleMultiviewOVR != NULL);

		// Just because the driver declares multi-view support and hands us valid function pointers doesn't actually guarantee the feature works...
		if (bSupportsMobileMultiView)
		{
			UE_LOG(LogRHI, Log, TEXT("Device supports mobile multi-view."));
		}
	}

	GSupportsDepthRenderTargetWithoutColorRenderTarget = true;
	
	if (IsES32Usable())
	{
		glTexBufferEXT = (PFNGLTEXBUFFEREXTPROC)((void*)eglGetProcAddress("glTexBuffer"));
		glTexBufferRangeEXT = (PFNGLTEXBUFFERRANGEEXTPROC)((void*)eglGetProcAddress("glTexBufferRange"));
		glCopyImageSubDataEXT = (PFNGLCOPYIMAGESUBDATAEXTPROC)((void*)eglGetProcAddress("glCopyImageSubData"));
		glEnableiEXT = (PFNGLENABLEIEXTPROC)((void*)eglGetProcAddress("glEnablei"));
		glDisableiEXT = (PFNGLDISABLEIEXTPROC)((void*)eglGetProcAddress("glDisablei"));
		glBlendEquationiEXT = (PFNGLBLENDEQUATIONIEXTPROC)((void*)eglGetProcAddress("glBlendEquationi"));
		glBlendEquationSeparateiEXT = (PFNGLBLENDEQUATIONSEPARATEIEXTPROC)((void*)eglGetProcAddress("glBlendEquationSeparatei"));
		glBlendFunciEXT = (PFNGLBLENDFUNCIEXTPROC)((void*)eglGetProcAddress("glBlendFunci"));
		glBlendFuncSeparateiEXT = (PFNGLBLENDFUNCSEPARATEIEXTPROC)((void*)eglGetProcAddress("glBlendFuncSeparatei"));
		glColorMaskiEXT = (PFNGLCOLORMASKIEXTPROC)((void*)eglGetProcAddress("glColorMaski"));
	}
	
	if (!glEnableiEXT && ExtensionsString.Contains(TEXT("GL_EXT_draw_buffers_indexed")))
	{
		// GL_EXT_draw_buffers_indexed
		glEnableiEXT = (PFNGLENABLEIEXTPROC)((void*)eglGetProcAddress("glEnableiEXT"));
		glDisableiEXT = (PFNGLDISABLEIEXTPROC)((void*)eglGetProcAddress("glDisableiEXT"));
		glBlendEquationiEXT = (PFNGLBLENDEQUATIONIEXTPROC)((void*)eglGetProcAddress("glBlendEquationiEXT"));
		glBlendEquationSeparateiEXT = (PFNGLBLENDEQUATIONSEPARATEIEXTPROC)((void*)eglGetProcAddress("glBlendEquationSeparateiEXT"));
		glBlendFunciEXT = (PFNGLBLENDFUNCIEXTPROC)((void*)eglGetProcAddress("glBlendFunciEXT"));
		glBlendFuncSeparateiEXT = (PFNGLBLENDFUNCSEPARATEIEXTPROC)((void*)eglGetProcAddress("glBlendFuncSeparateiEXT"));
		glColorMaskiEXT = (PFNGLCOLORMASKIEXTPROC)((void*)eglGetProcAddress("glColorMaskiEXT"));
	}
	bSupportsDrawBuffersBlend = (glEnableiEXT != nullptr);
		
	if (!glTexBufferEXT && ExtensionsString.Contains(TEXT("GL_EXT_texture_buffer")))
	{
		// GL_EXT_texture_buffer
		glTexBufferEXT = (PFNGLTEXBUFFEREXTPROC)((void*)eglGetProcAddress("glTexBufferEXT"));
		glTexBufferRangeEXT = (PFNGLTEXBUFFERRANGEEXTPROC)((void*)eglGetProcAddress("glTexBufferRangeEXT"));
	}

	if (!glCopyImageSubDataEXT && ExtensionsString.Contains(TEXT("GL_EXT_copy_image")))
	{
		// GL_EXT_copy_image
		glCopyImageSubDataEXT = (PFNGLCOPYIMAGESUBDATAEXTPROC)((void*)eglGetProcAddress("glCopyImageSubDataEXT"));
	}
	bSupportsCopyImage = (glCopyImageSubDataEXT != nullptr);

	// Set lowest possible limits for texture units, to avoid extra work in GL RHI
	MaxTextureImageUnits = FMath::Min(MaxTextureImageUnits, 16);
	MaxVertexTextureImageUnits = FMath::Min(MaxVertexTextureImageUnits, 16);
	MaxCombinedTextureImageUnits = FMath::Min(MaxCombinedTextureImageUnits, 32);

	if (bSupportsBGRA8888)
	{
		// Check whether device supports BGRA as color attachment
		GLuint FrameBuffer;
		glGenFramebuffers(1, &FrameBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, FrameBuffer);
		GLuint BGRA8888Texture;
		glGenTextures(1, &BGRA8888Texture);
		glBindTexture(GL_TEXTURE_2D, BGRA8888Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, 256, 256, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, BGRA8888Texture, 0);

		bSupportsBGRA8888RenderTarget = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		glDeleteTextures(1, &BGRA8888Texture);
		glDeleteFramebuffers(1, &FrameBuffer);
	}
}

#endif

#endif //desktop
