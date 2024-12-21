#pragma once

#include <iostream>
#include <deque>

class DLLEXPORT FPerformanceDetector
{
public:
	float GetAverageFPS();
	float GetInferenceSpeed();
	float GetDecodeTime();
	int GetFrameBeginTime();
	int GetDecodedTokensNum();
	TAtomic<float>& GetTimeSlice();

	static FPerformanceDetector& GetInstance();

	void SetFrameBeginTime(const int Time);
	void SetTimeSlice(const float Time);
	void SetFrameTime(const int Time);
	float GetFrameTime();

	void ChangeTimeSlice(const float Time);
	void ComputeInferenceSpeed();
	void SetStartInferenceTime(const float Time);
	void SetDecodedTokensNum(const int Num);

	float GetTargetFPS();
	float GetTargetInferenceSpeed();
	void SetTargetFPS(const float FPS);
	void SetTargetInferenceSpeed(const float Speed);

	void SetInferenceSpeed(const float Speed);

private:
	FPerformanceDetector();
	float StartInferenceTime;
	float InferenceSpeed;
	int DecodedTokensNum;
	int FrameBeginTime;
	int FrameRenderingEndTime;
	TAtomic<int> FrameTime;
	TAtomic<float> DecodeTime;
	int NumPrimitives;
	TAtomic<float> TimeSlice;
	TAtomic<float> TargetFPS;
	TAtomic<float> TargetInferenceSpeed;
};