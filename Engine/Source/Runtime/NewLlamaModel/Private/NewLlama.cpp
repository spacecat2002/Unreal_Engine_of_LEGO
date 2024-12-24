#include "..\Public\NewLlama.h"

#include "Editor/UnrealEdEngine.h"
#include "llamacpp/ggml-cuda.h"

FNewLlamaModel& FNewLlamaModel::GetInstance()
{
	static FNewLlamaModel llama;
	return llama;
}


void FNewLlamaModel::Init()
{
	fprintf(stderr, "FNewLlamaModel::Init()\n");	

	std::string model_path;
	
	model_path = "E:\\Program Files\\Llama-3.2-8B-Instruct.Q8_0.gguf";
	
	n_len = 250;
	
	llama_backend_init();
	
	model_params = llama_model_default_params();
	model_params.n_gpu_layers = 10000;
	model = llama_load_model_from_file(model_path.c_str(), model_params);
	
	ctx_params = llama_context_default_params();
	ctx_params.n_ctx = 2048;
	ctx_params.no_perf = false;
	ctx_params.flash_attn = true;

	auto sparams = llama_sampler_chain_default_params();
	sparams.no_perf = false;
	smpl = llama_sampler_chain_init(sparams);
	llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
}

void FNewLlamaModel::Exit()
{
	llama_free(ctx);
	llama_free_model(model);
	llama_sampler_free(smpl);

	fprintf(stderr, "Llama has exited.\n");
}


void FNewLlamaModel::Inference(std::string prompt, FString* content)
{
	
	FTaskDetector::GetInstance().SetIsInInferring(true);
	//params.prompt = input_prompt;
	
	const int n_prompt = -llama_tokenize(model, prompt.c_str(), prompt.size(), NULL, 0, true, true);
	std::vector<llama_token> tokens_list(n_prompt);
	if (llama_tokenize(model, prompt.c_str(), prompt.size(), tokens_list.data(), tokens_list.size(), true, true) < 0) {
		fprintf(stderr, "%s: error: failed to tokenize the prompt\n", __func__);
		return;
	}
	
	ctx_params.n_batch = n_prompt;
	ctx = llama_new_context_with_model(model, ctx_params);
	
    // print the prompt token-by-token

	for (auto id : tokens_list) {
		char buf[128];
		int n = llama_token_to_piece(model, id, buf, sizeof(buf), 0, true);
		if (n < 0) {
			fprintf(stderr, "%s: error: failed to convert token to piece\n", __func__);
			return;
		}
		std::string s(buf, n);
		printf("%s", s.c_str());
	}

    // create a llama_batch with size 512
    // we use this object to submit token data for decoding

	llama_batch batch = llama_batch_get_one(tokens_list.data(), tokens_list.size());

	const auto t_main_start = ggml_time_us();
	double start_time = FDateTime::Now().GetTimeOfDay().GetTotalSeconds();
	FPerformanceDetector::GetInstance().SetStartInferenceTime(start_time);
	
	int n_decode = 0;
	llama_token new_token_id;
	
    // main loop
	
	std::string response = "";
	
    for (int n_pos = 0; n_pos + batch.n_tokens < n_prompt + n_len; ) {
    	//if (AutoSchedule) FTaskScheduler::GetInstance().CheckPerformance();

    	if (n_pos == 0)
    	{
    		if (llama_decode(ctx, batch)) {
    		 	fprintf(stderr, "%s : failed to eval, return code %d\n", __func__, 1);
    		 	return;
    		}
    	}
    	else
    	{
    		double token_start_time = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
    		FTaskScheduler::GetInstance().SetTokenBeginTime(token_start_time);
    	
    		if (llama_decode_skip_layers(ctx, batch)) {
    			fprintf(stderr, "%s : failed to eval, return code %d\n", __func__, 1);
    			return;
    		}
    	}
    	
    	n_pos += batch.n_tokens;

    	// sample the next token
	    {
		    new_token_id = llama_sampler_sample(smpl, ctx, -1);

    		n_decode += 1;

    		FPerformanceDetector::GetInstance().SetDecodedTokensNum(n_decode);
    		FPerformanceDetector::GetInstance().ComputeInferenceSpeed();

    		//std::string s(buf, n);
    		std::string s = "generated token: ";
    		s += std::to_string(n_decode);
    		response = s.c_str();
        	
    		*content = UTF8_TO_TCHAR(response.c_str());

    		// is it an end of generation?
    		if (llama_token_is_eog(model, new_token_id)) {
    			break;
    		}

    		char buf[128];
    		int n = llama_token_to_piece(model, new_token_id, buf, sizeof(buf), 0, true);
    		if (n < 0) {
    			fprintf(stderr, "%s: error: failed to convert token to piece\n", __func__);
    			return;
    		}

    		// prepare the next batch with the sampled token
    		batch = llama_batch_get_one(&new_token_id, 1);
	    }
    }
	FTaskDetector::GetInstance().SetIsInInferring(false);
	

	printf("\n");

	const auto t_main_end = ggml_time_us();

	fprintf(stderr, "%s: decoded %d tokens in %.2f s, speed: %.2f t/s\n",
			__func__, n_decode, (t_main_end - t_main_start) / 1000000.0f, n_decode / ((t_main_end - t_main_start) / 1000000.0f));

	fprintf(stderr, "\n");
	llama_perf_sampler_print(smpl);
	llama_perf_context_print(ctx);
	fprintf(stderr, "\n");
	
	llama_free(ctx);

	AvgInferenceTime = (t_main_end - t_main_start) / 1000.0f / n_decode;

	fprintf(stderr, "\n");
}

float FNewLlamaModel::GetInferenceTime()
{
	return AvgInferenceTime;
}

