#pragma once
#include "../Public/TaskScheduler.h"
#include "Async/RHIThreadWaiting.h"

FTaskScheduler::FTaskScheduler()
	: Performance(0)
	, SkipDepth(0)
	, DecodePhase(0)
	, TokenBeginTime(0)
	, TokenEndTime(0)
	, TokenDecodeTime(0)
	, TokenBeginInFrameEnd(0)
	, TokenNowLayer(0)
	, SLOTime(50)
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

void FTaskScheduler::SetMaxFPS(const int FPS)
{
	MaxFPS = FPS;
}

void FTaskScheduler::SetSLOTime(const float Time)
{
	SLOTime = Time;
}

void FTaskScheduler::SetTokenBeginTime(const float Time)
{
	TokenBeginTime = Time;
	if (!FTaskDetector::GetInstance().GetIsInFrameRendering())
	{
		TokenBeginInFrameEnd = 1;
	}
}

void FTaskScheduler::SetTokenEndTime(const float Time)
{
	TokenEndTime = Time;
}

void FTaskScheduler::SetBeginInFrameEnd()
{
	TokenBeginInFrameEnd = 0;
}


void FTaskScheduler::UpdateSkipDepth()
{
	double NowTime = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
	const float TimeSlice = FPerformanceDetector::GetInstance().GetTimeSlice();
	double TokenAllTime = NowTime + TimeSlice - TokenBeginTime;
	double TokenRemainTime = 0;
	if (TokenAllTime < SLOTime)
	{
		if (TokenAllTime + 1000/MaxFPS.Load() > SLOTime)
		{
			
		}
	} 
	double TokenAllDecodeTime = TokenDecodeTime + TimeSlice;
}

void FTaskScheduler::CanInfer()
{

	while (true)
	{
		bool bIsInFrameRendering = FTaskDetector::GetInstance().GetIsInFrameRendering(); //当前帧的所有渲染工作是否完成
		const bool bRenderingTaskIsRunning = FTaskDetector::GetInstance().GetRenderingTaskIsRunning();
		const bool bRHITaskIsRunning = FTaskDetector::GetInstance().GetRHITaskIsRunning();
		if (bIsInFrameRendering)
		{
			if (!bRenderingTaskIsRunning && !bRHITaskIsRunning && DecodePhase)
			{
				return;
			}
		}
		else
		{
			if (!bRenderingTaskIsRunning && !bRHITaskIsRunning)
			{
				return;
			}
		}
	}	
}


