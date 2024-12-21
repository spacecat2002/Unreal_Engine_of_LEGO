#pragma once

#include "SchedulingSystem/DurationPredictor/Public/DurationPredictor.h"
#include "SchedulingSystem/TaskDetector/Public/TaskDetector.h"
#include "SchedulingSystem/PerformanceDetector/Public/PerformanceDetector.h"

class DLLEXPORT FTaskScheduler
{
public:
	std::fstream f1, f2;
	static FTaskScheduler& GetInstance();
    //void CanRender();
    void CanInfer();
	int GetSkipDepth();
	void SetSkipDepth(const int SkipDepth);
	void SetDecodePhase(const int Phase);
	~FTaskScheduler();
private:
	FTaskScheduler();
	int State;
	TAtomic<int> Performance;
	TAtomic<int> SkipDepth;
	TAtomic<int> DecodePhase;
};
