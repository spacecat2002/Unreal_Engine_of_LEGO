#include "../Public/TaskScheduler.h"

FTaskScheduler::FTaskScheduler()
	: TargetInferenceSpeed(0)
	, TargetMaxFPS(0)
	, Performance(0)
	, State(0)
{
}

FTaskScheduler& FTaskScheduler::GetInstance()
{
    static FTaskScheduler Instance;
    return Instance;
}

float FTaskScheduler::GetTargetInferenceSpeed()
{
	return TargetInferenceSpeed;
}

float FTaskScheduler::GetTargetMaxFPS()
{
	return TargetMaxFPS;
}

int FTaskScheduler::GetSchedulerState()
{
	return State;
}

void FTaskScheduler::SetTargetInferenceSpeed(const float Speed)
{
	TargetInferenceSpeed = Speed;
}

void FTaskScheduler::SetTargetMaxFPS(const float FPS)
{
	TargetMaxFPS = FPS;
}

void FTaskScheduler::SetSchedulerState(const int nState)
{
	State = nState;
}

void FTaskScheduler::ComparePerformance() 
{ //在每帧渲染结束以及每个token生成完成后调用该函数，Performance=1时说明FPS较低，需要调整推理算子的调度
 	const float FPS = FPerformanceDetector::GetInstance().GetAverageFPS(); //当前FPS
 	const float InferenceSpeed = FPerformanceDetector::GetInstance().GetInferenceSpeed(); //当前推理速度
	const float DeltaFPS = (FPS - TargetMaxFPS) / TargetMaxFPS; //FPS误差百分比
	const float DeltaInferenceSpeed = (InferenceSpeed - TargetInferenceSpeed) / TargetInferenceSpeed; //推理速度误差百分比

	if (DeltaInferenceSpeed > 0.5) Performance = 0; //推理刚开始时，时间较短，速度过大，处于不稳定状态，不做处理
	else if (DeltaFPS < 0 && DeltaInferenceSpeed > 0) Performance = 1; //FPS低于目标且推理速度高于目标，须调整推理算子调度
	else if (DeltaFPS < 0 && DeltaInferenceSpeed < 0) 
	{ //FPS和推理速度均低于目标时，DeltaFPS越小离目标的误差越大，须调整推理算子调度
		if (DeltaFPS < DeltaInferenceSpeed) Performance = 1;
		else Performance = 0;
	}
	else Performance = 0;
}

void FTaskScheduler::CanInfer(TArray<int>& Input, FString& Name)
{
    bool bIsInFrameRendering = FTaskDetector::GetInstance().GetIsInFrameRendering(); //当前帧的所有渲染工作是否完成
	while (true)
	{
		if (bIsInFrameRendering) { //如果当前帧的渲染工作没有完成
			while (true)
			{
				const bool bIsRenderingEnd = FTaskDetector::GetInstance().GetIsRenderingEnd();
				const bool bIsInRHITask = FTaskDetector::GetInstance().GetIsInRHITask();
				if (bIsRenderingEnd) //某次渲染任务已经完成
				{
					if (State == 1) //策略1，某次渲染任务完成就可以调度推理算子
						return;
					
					if (State == 2) //策略2，某次渲染任务完成后，如果FPS较低时，只有没有其他RHI任务执行时才调度推理算子，如果FPS较高，直接调度推理算子
						if (!(Performance.Load() && bIsInRHITask)) return;

					if (State == 3) //策略3，某次渲染任务完成且没有其他RHI任务正在执行时就调度推理算子
						if (!bIsInRHITask) return;
				}
				bIsInFrameRendering = FTaskDetector::GetInstance().GetIsInFrameRendering(); 
				if (!bIsInFrameRendering) break; //更新帧渲染状态，如果一帧的所有渲染工作已经完成就退出该循环
			}
		}
		else { //如果当前帧的渲染工作已经完成
			const float TimeSlice = FPerformanceDetector::GetInstance().GetTimeSlice();
			const float Duration = FDurationPredictor::GetInstance(Name).GetDuration(Input) / 1000.0;
			if (TimeSlice >= Duration) //空闲时间片长度大于预测推理时间
			{
				FPerformanceDetector::GetInstance().ChangeTimeSlice(-Duration); //修改空闲时间长度
				return; //调度推理算子执行
			}
			bIsInFrameRendering = FTaskDetector::GetInstance().GetIsInFrameRendering(); //更新帧渲染状态
		}
	}
}


