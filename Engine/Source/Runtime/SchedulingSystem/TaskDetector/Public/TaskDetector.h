#pragma once

class DLLEXPORT FTaskDetector
{
public:
	static FTaskDetector& GetInstance();
	TAtomic<bool>& GetIsRenderingEnd();
	TAtomic<bool>& GetIsInFrameRendering();
	TAtomic<bool>& GetIsInRHITask();
	TAtomic<bool>& GetIsInInferring();
	void SetIsRenderingEnd(const bool State);
	void SetIsInFrameRendering(const bool State);
	void SetIsInInferring(const bool State);
	void SetIsInRHITask(const bool State);
private:
	FTaskDetector();
	TAtomic<bool> bIsInRHITask;
	TAtomic<bool> bIsRenderingEnd;
	TAtomic<bool> bIsInFrameRendering;
	TAtomic<bool> bIsInInferring;
};
