#pragma once

#include "SchedulingSystem/DurationPredictor/Public/DurationPredictor.h"
#include "SchedulingSystem/TaskDetector/Public/TaskDetector.h"
#include "SchedulingSystem/PerformanceDetector/Public/PerformanceDetector.h"

class DLLEXPORT FTaskScheduler
{
public:
	std::fstream f1, f2;
	static FTaskScheduler& GetInstance();
    void CanInfer();
	int GetSkipDepth();
	void SetSkipDepth(const int SkipDepth);
	void SetDecodePhase(const int Phase);
	void SetMaxFPS(const int FPS);
	void SetSLOTime(const float Time);
	void SetTokenBeginTime(const float Time);
	void SetTokenEndTime(const float Time);
	void SetTokenNowLayer(const int Layer);
	void SetBeginInFrameEnd();
	void UpdateTokenDecodeTime();
	void UpdateSkipDepth();
	~FTaskScheduler();
private:
	FTaskScheduler();
	TAtomic<int> Performance;
	TAtomic<int> SkipDepth;
	TAtomic<int> DecodePhase;
	TAtomic<float> MaxFPS;
	TAtomic<double> TokenBeginTime;
	TAtomic<double> TokenEndTime;
	TAtomic<double> TokenDecodeTime;
	TAtomic<int> TokenNowLayer;
	bool TokenBeginInFrameEnd;
	TAtomic<int> AllGraphNodes;
	TAtomic<int> NowGraphNodes;
	float SLOTime;
};
