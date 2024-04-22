#pragma once

class DLLEXPORT FPerformanceDetector
{
public:
	float GetAverageFPS();
	float GetInferenceSpeed();

	float GetFrameTime();
	TAtomic<float>& GetTimeSlice();

	static FPerformanceDetector& GetInstance();

	void SetFrameBeginTime(const float Time);
	void SetFrameEndTime(const float Time);
	void SetTimeSlice(const float Time);

	void ChangeTimeSlice(const float Time);
	void ComputeFrameTime();
	void ComputeInferenceSpeed();
	void SetStartInferenceTime(const float Time);
	void SetDecodedTokensNum(const int Num);

private:
	FPerformanceDetector();
	float StartInferenceTime;
	float InferenceSpeed;
	int DecodedTokensNum;
	float FrameBeginTime;
	float FrameEndTime;
	float FrameTime;
	TAtomic<float> TimeSlice;
};
