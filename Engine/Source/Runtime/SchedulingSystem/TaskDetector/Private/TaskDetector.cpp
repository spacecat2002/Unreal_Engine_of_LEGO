#include "..\Public\TaskDetector.h"

FTaskDetector::FTaskDetector()
	: bIsRenderingEnd(true)
	, bIsInFrameRendering(false)
	, bIsInInferring(false)
	, bIsInRHITask(false)
{
}

FTaskDetector& FTaskDetector::GetInstance()
{
	static FTaskDetector Instance;
	return Instance;
}

TAtomic<bool>& FTaskDetector::GetIsRenderingEnd()
{
	return bIsRenderingEnd;
}

TAtomic<bool>& FTaskDetector::GetIsInFrameRendering()
{
	return bIsInFrameRendering;
}

TAtomic<bool>& FTaskDetector::GetIsInInferring()
{
	return bIsInInferring;
}

TAtomic<bool>& FTaskDetector::GetIsInRHITask()
{
	return bIsInRHITask;
}

void FTaskDetector::SetIsRenderingEnd(const bool State)
{
	bIsRenderingEnd = State;
}

void FTaskDetector::SetIsInFrameRendering(const bool State)
{
	bIsInFrameRendering = State;
}

void FTaskDetector::SetIsInInferring(const bool State)
{
	bIsInInferring = State;
}

void FTaskDetector::SetIsInRHITask(const bool State)
{
	bIsInRHITask = State;
}




















