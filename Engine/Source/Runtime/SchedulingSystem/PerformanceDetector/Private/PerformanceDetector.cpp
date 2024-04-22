#include "PerformanceDetector.h"
#include "Windows.h"

FPerformanceDetector::FPerformanceDetector()
	: StartInferenceTime(0)
	, DecodedTokensNum(0)
	, FrameBeginTime(0)
	, FrameEndTime(0)
	, FrameTime(0)
	, TimeSlice(0)
{
}

FPerformanceDetector& FPerformanceDetector::GetInstance()
{
	static FPerformanceDetector Instance;
	return Instance;
}

float FPerformanceDetector::GetAverageFPS()
{
	extern ENGINE_API float GAverageFPS;
	return GAverageFPS;
}

// float FPerformanceDetector::GetMaxFPS()
// {
// 	return MaxFPS;
// }

float FPerformanceDetector::GetInferenceSpeed()
{
	return InferenceSpeed;
}

float FPerformanceDetector::GetFrameTime()
{
	return FrameTime;
}

TAtomic<float>& FPerformanceDetector::GetTimeSlice()
{
	return TimeSlice;
}

void FPerformanceDetector::ComputeInferenceSpeed()
{
	const float NowTime = FDateTime::Now().GetTimeOfDay().GetTotalSeconds();
	InferenceSpeed = 1.0 * DecodedTokensNum / (NowTime - StartInferenceTime);
}


void FPerformanceDetector::SetStartInferenceTime(const float Time)
{
	StartInferenceTime = Time;
}

void FPerformanceDetector::SetDecodedTokensNum(const int Num)
{
	DecodedTokensNum = Num;
}

void FPerformanceDetector::SetFrameBeginTime(const float Time)
{
	FrameBeginTime = Time;
}

void FPerformanceDetector::SetFrameEndTime(const float Time)
{
	FrameEndTime = Time;
}

void FPerformanceDetector::SetTimeSlice(const float Time)
{
	TimeSlice = Time;
}

void FPerformanceDetector::ChangeTimeSlice(const float Time)
{
	TimeSlice = TimeSlice + Time;
}

void FPerformanceDetector::ComputeFrameTime()
{
	LARGE_INTEGER Freq;
	QueryPerformanceFrequency(&Freq);
	FrameTime = (FrameEndTime - FrameBeginTime) * 1000.0 / (float)Freq.QuadPart;
}



