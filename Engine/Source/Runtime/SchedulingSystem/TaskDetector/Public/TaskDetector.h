#pragma once

class FRenderTaskDetector: public FRunnable
{
public:
	
	virtual bool Init() { return true; }
	virtual uint32_t Run();
	virtual void Exit() {}
};

class DLLEXPORT FTaskDetector
{
public:
	FRenderTaskDetector* Detector;
	static FTaskDetector& GetInstance();
	TAtomic<bool>& GetRenderingTaskIsRunning();
	TAtomic<bool>& GetIsInFrameRendering();
	TAtomic<bool>& GetRHITaskIsRunning();
	TAtomic<bool>& GetIsInInferring();
	void SetRenderingTaskIsRunning(const bool State);
	void SetIsInFrameRendering(const bool State);
	void SetIsInInferring(const bool State);
	void SetRHITaskIsRunning(const bool State);
private:
	FTaskDetector();
	TAtomic<bool> bRHITaskIsRunning;
	TAtomic<bool> bRenderingTaskIsRunning;
	TAtomic<bool> bIsInFrameRendering;
	TAtomic<bool> bIsInInferring;
};
