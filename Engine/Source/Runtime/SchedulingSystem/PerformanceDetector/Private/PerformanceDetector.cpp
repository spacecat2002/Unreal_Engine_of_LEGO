#include "PerformanceDetector.h"
#include "Windows.h"

FPerformanceDetector::FPerformanceDetector()
	: StartInferenceTime(0)
	, DecodedTokensNum(0)
	, FrameBeginTime(0)
	, FrameRenderingEndTime(0)
	, FrameTime(0)
	, TimeSlice(0)
	, TargetFPS(0)
	, TargetInferenceSpeed(0)
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

float FPerformanceDetector::GetInferenceSpeed()
{
	return InferenceSpeed;
}

int FPerformanceDetector::GetFrameBeginTime()
{
	return FrameBeginTime;
}

float FPerformanceDetector::GetDecodeTime()
{
	return DecodeTime;
}

TAtomic<float>& FPerformanceDetector::GetTimeSlice()
{
	if (FrameTime == 0)
	{
		FrameRenderingEndTime = FPlatformTime::Cycles64();
		
		LARGE_INTEGER Freq;
		QueryPerformanceFrequency(&Freq);
		FrameTime = (FrameRenderingEndTime - FrameBeginTime) * 1000.0f / (float)Freq.QuadPart;
		const float nTimeSlice = 1000.0/GetAverageFPS() - FrameTime;
		TimeSlice = FMath::Max(nTimeSlice, 0.0f);
	}
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

void FPerformanceDetector::SetFrameBeginTime(const int Time)
{
	FrameBeginTime = Time;
}

void FPerformanceDetector::SetTimeSlice(const float Time)
{
	TimeSlice = Time;
}

void FPerformanceDetector::ChangeTimeSlice(const float Time)
{
	TimeSlice = TimeSlice + Time;
}

void FPerformanceDetector::SetFrameTime(const int Time)
{
	FrameTime = Time;
}

float FPerformanceDetector::GetFrameTime()
{
	return FrameTime;
}

float FPerformanceDetector::GetTargetFPS()
{
	return TargetFPS;
}

float FPerformanceDetector::GetTargetInferenceSpeed()
{
	return TargetInferenceSpeed;
}

void FPerformanceDetector::SetTargetFPS(const float FPS)
{
	TargetFPS = FPS;
}

void FPerformanceDetector::SetTargetInferenceSpeed(const float Speed)
{
	TargetInferenceSpeed = Speed;
}

void FPerformanceDetector::SetInferenceSpeed(const float Speed)
{
	InferenceSpeed = Speed;
}



















