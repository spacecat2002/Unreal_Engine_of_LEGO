#pragma once

#include "CoreMinimal.h"
#include <string>
#include "../Private/llamacpp/llama.h"
#include "../../SchedulingSystem/PerformanceDetector/Public/PerformanceDetector.h"
#include "../../SchedulingSystem/TaskDetector/Public/TaskDetector.h"

class DLLEXPORT FNewLlamaModel
{
public:
	static FNewLlamaModel& GetInstance();
	void Init();
	void Exit();
	void Inference(std::string prompt, FString* content, bool AutoSchedule);
	float GetInferenceSpeed();
	std::string prompt;

private:
	FNewLlamaModel() {};
	FNewLlamaModel(const FNewLlamaModel&) = delete;
	FNewLlamaModel& operator=(const FNewLlamaModel&) = delete;
	
	llama_model_params model_params;
	llama_model* model;
	llama_context_params ctx_params;
	llama_context* ctx;
	int n_len;
	float InferenceSpeed;
};










