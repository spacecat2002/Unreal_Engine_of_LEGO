#include "..\Public\TaskDetector.h"
#include "HAL/RunnableThread.h"
#include "Windows.h"

DECLARE_STATS_GROUP(TEXT("Rendering"), STATGROUP_LLAMA, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Rendering Task time"), STAT_Rendering, STATGROUP_LLAMA, );
DEFINE_STAT(STAT_Rendering);

uint32 FRenderTaskDetector::Run()
{
	while (true)
	{
		if (!FTaskDetector::GetInstance().GetRenderingTaskIsRunning())
		{
			SCOPE_CYCLE_COUNTER(STAT_Rendering);
			while (!FTaskDetector::GetInstance().GetRenderingTaskIsRunning());
		}
	}
	return 0;
}

FTaskDetector::FTaskDetector()
	: bRenderingTaskIsRunning(false)
	, bIsInFrameRendering(false)
	, bIsInInferring(false)
	, bRHITaskIsRunning(false)
{
	Detector = new FRenderTaskDetector();
	//FRunnableThread::Create(Detector, TEXT("RenderingTask"), 0, TPri_Normal);
}

FTaskDetector& FTaskDetector::GetInstance()
{
	static FTaskDetector Instance;
	return Instance;
}

TAtomic<bool>& FTaskDetector::GetRenderingTaskIsRunning()
{
	return bRenderingTaskIsRunning;
}

TAtomic<bool>& FTaskDetector::GetIsInFrameRendering()
{
	return bIsInFrameRendering;
}

TAtomic<bool>& FTaskDetector::GetIsInInferring()
{
	return bIsInInferring;
}

TAtomic<bool>& FTaskDetector::GetRHITaskIsRunning()
{
	return bRHITaskIsRunning;
}

void FTaskDetector::SetRenderingTaskIsRunning(const bool State)
{
	bRenderingTaskIsRunning = State;
}

void FTaskDetector::SetIsInFrameRendering(const bool State)
{
	bIsInFrameRendering = State;
}

void FTaskDetector::SetIsInInferring(const bool State)
{
	bIsInInferring = State;
}

void FTaskDetector::SetRHITaskIsRunning(const bool State)
{
	bRHITaskIsRunning = State;
}


























