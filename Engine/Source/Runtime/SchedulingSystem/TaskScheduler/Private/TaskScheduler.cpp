#pragma once
#include "../Public/TaskScheduler.h"
#include "Async/RHIThreadWaiting.h"

FTaskScheduler::FTaskScheduler()
	: Performance(0)
	, State(4)
	, SkipDepth(0)
	, DecodePhase(0)
{
}

FTaskScheduler::~FTaskScheduler()
{
}

FTaskScheduler& FTaskScheduler::GetInstance()
{
    static FTaskScheduler Instance;
    return Instance;
}

int FTaskScheduler::GetSkipDepth()
{
	return SkipDepth;
}

void FTaskScheduler::SetSkipDepth(const int nSkipDepth)
{
	SkipDepth = nSkipDepth;
}

void FTaskScheduler::SetDecodePhase(const int Phase)
{
	DecodePhase = Phase;
}

void FTaskScheduler::CanInfer()
{

	while (true)
	{
		bool bIsInFrameRendering = FTaskDetector::GetInstance().GetIsInFrameRendering(); //当前帧的所有渲染工作是否完成
		const bool bRenderingTaskIsRunning = FTaskDetector::GetInstance().GetRenderingTaskIsRunning();
		const bool bRHITaskIsRunning = FTaskDetector::GetInstance().GetRHITaskIsRunning();
		if (!bRenderingTaskIsRunning && !bRHITaskIsRunning)
		{
			if (bIsInFrameRendering && DecodePhase)
			{
				return;
			}
			
			if (!bIsInFrameRendering)
			{
				const float TimeSlice = FPerformanceDetector::GetInstance().GetTimeSlice();
			}
		}
	}	
}


