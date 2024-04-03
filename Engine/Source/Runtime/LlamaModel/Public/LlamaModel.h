#pragma once

#include "CoreMinimal.h"
#include "../Private/llama.cpp/common/common.h"
#include "../Private/llama.cpp/llama.h"
#include "../../SchedulingSystem/PerformanceDetector/Public/PerformanceDetector.h"

class DLLEXPORT FLlamaModel
{
public:
	static FLlamaModel& GetInstance();
	void Init();
	void Exit();
	void Inference(std::string prompt, FString* content);
	std::string prompt;

private:
	FLlamaModel() {};
	FLlamaModel(const FLlamaModel&) = delete;
	FLlamaModel& operator=(const FLlamaModel&) = delete;
	
	gpt_params params;
	llama_model_params model_params;
	llama_model* model;
	llama_context_params ctx_params;
	llama_context* ctx;
	int n_len;
};










