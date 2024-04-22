#pragma once

#include "SchedulingSystem/DurationPredictor/Public/DurationPredictor.h"
#include "SchedulingSystem/TaskDetector/Public/TaskDetector.h"
#include "SchedulingSystem/PerformanceDetector/Public/PerformanceDetector.h"

class DLLEXPORT FTaskScheduler
{
public:
	static FTaskScheduler& GetInstance();
    void CanRender();
    void CanInfer(TArray<int>& Input, FString& Name);
	float GetTargetInferenceSpeed();
	float GetTargetMaxFPS();
	void SetTargetInferenceSpeed(const float Speed);
	void SetTargetMaxFPS(const float FPS);
	void ComparePerformance();
	void SetSchedulerState(const int State);
	int GetSchedulerState();
private:
	FTaskScheduler();
	float TargetInferenceSpeed;
	float TargetMaxFPS;
	int State;
	TAtomic<int> Performance;
};
