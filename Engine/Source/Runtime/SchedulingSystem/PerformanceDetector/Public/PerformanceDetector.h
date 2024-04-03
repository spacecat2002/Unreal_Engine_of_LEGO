#pragma once

class DLLEXPORT FPerformanceDetector
{
public:
	float GetAverageFPS();
	double GetInferenceSpeed();
	void SetStartInferenceTime(double Time);
	void SetDecodedTokensNum(int Num);
	static FPerformanceDetector& GetInstance();
private:
	FPerformanceDetector();
	double StartInferenceTime;
	int DecodedTokensNum;
};
