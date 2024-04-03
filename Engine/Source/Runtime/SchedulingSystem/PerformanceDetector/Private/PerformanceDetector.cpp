#include "PerformanceDetector.h"
#include "Engine/Public/UnrealEngine.h"

FPerformanceDetector::FPerformanceDetector()
{
	StartInferenceTime = 0;
	DecodedTokensNum = 0;
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

double FPerformanceDetector::GetInferenceSpeed()
{
	double NowTime = FDateTime::Now().GetTimeOfDay().GetTotalSeconds();
	return (double)DecodedTokensNum / (NowTime - StartInferenceTime);
}

void FPerformanceDetector::SetStartInferenceTime(double Time)
{
	StartInferenceTime = Time;
}

void FPerformanceDetector::SetDecodedTokensNum(int Num)
{
	DecodedTokensNum = Num;
}



