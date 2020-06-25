// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGraphEvent.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphPrivate.h"
#include "RenderGraphPass.h"

RENDERCORE_API bool GetEmitRDGEvents()
{
	check(IsInRenderingThread());
#if RDG_EVENTS != RDG_EVENTS_NONE
	return GetEmitDrawEvents() || GRDGDebug;
#else
	return false;
#endif
}

#if RDG_EVENTS == RDG_EVENTS_STRING_COPY

FRDGEventName::FRDGEventName(const TCHAR* InEventFormat, ...)
	: EventFormat(InEventFormat)
{
	check(InEventFormat);

	if (GetEmitRDGEvents())
	{
		va_list VAList;
		va_start(VAList, InEventFormat);
		TCHAR TempStr[256];
		// Build the string in the temp buffer
		FCString::GetVarArgs(TempStr, UE_ARRAY_COUNT(TempStr), InEventFormat, VAList);
		va_end(VAList);

		FormatedEventName = TempStr;
	}
}

#endif

static void GetEventScopePathRecursive(const FRDGEventScope* Root, FString& String)
{
	if (Root->ParentScope)
	{
		GetEventScopePathRecursive(Root->ParentScope, String);
	}

	if (!String.IsEmpty())
	{
		String += TEXT(".");
	}

	String += Root->Name.GetTCHAR();
}

FString FRDGEventScope::GetPath(const FRDGEventName& Event) const
{
	FString Path;
	GetEventScopePathRecursive(this, Path);
	Path += TEXT(".");
	Path += Event.GetTCHAR();
	return MoveTemp(Path);
}

FRDGEventScopeGuard::FRDGEventScopeGuard(FRDGBuilder& InGraphBuilder, FRDGEventName&& ScopeName, bool InbCondition)
	: GraphBuilder(InGraphBuilder)
	, bCondition(InbCondition)
{
	if (bCondition)
	{
		GraphBuilder.BeginEventScope(MoveTemp(ScopeName));
	}
}

FRDGEventScopeGuard::~FRDGEventScopeGuard()
{
	if (bCondition)
	{
		GraphBuilder.EndEventScope();
	}
}

static void OnPushEvent(FRHIComputeCommandList& RHICmdList, const FRDGEventScope* Scope)
{
	RHICmdList.PushEvent(Scope->Name.GetTCHAR(), FColor(0));
}

static void OnPopEvent(FRHIComputeCommandList& RHICmdList)
{
	RHICmdList.PopEvent();
}

bool FRDGEventScopeStack::IsEnabled()
{
#if RDG_EVENTS
	return GetEmitRDGEvents();
#else
	return false;
#endif
}

FRDGEventScopeStack::FRDGEventScopeStack(FRHIComputeCommandList& InRHICmdList)
	: ScopeStack(InRHICmdList, &OnPushEvent, &OnPopEvent)
{}

void FRDGEventScopeStack::BeginScope(FRDGEventName&& EventName)
{
	if (IsEnabled())
	{
		ScopeStack.BeginScope(Forward<FRDGEventName&&>(EventName));
	}
}

void FRDGEventScopeStack::EndScope()
{
	if (IsEnabled())
	{
		ScopeStack.EndScope();
	}
}

void FRDGEventScopeStack::BeginExecute()
{
	if (IsEnabled())
	{
		ScopeStack.BeginExecute();
	}
}

void FRDGEventScopeStack::BeginExecutePass(const FRDGPass* Pass)
{
	if (IsEnabled())
	{
		ScopeStack.BeginExecutePass(Pass->GetScopes().Event);

		// Skip empty strings.
		const TCHAR* Name = Pass->GetEventName().GetTCHAR();

		if (Name && *Name)
		{
			FColor Color(255, 255, 255);
			ScopeStack.RHICmdList.PushEvent(Name, Color);
			bEventPushed = true;
		}
	}
}

void FRDGEventScopeStack::EndExecutePass()
{
	if (IsEnabled() && bEventPushed)
	{
		ScopeStack.RHICmdList.PopEvent();
		bEventPushed = false;
	}
}

void FRDGEventScopeStack::EndExecute()
{
	if (IsEnabled())
	{
		ScopeStack.EndExecute();
	}
}

FRDGGPUStatScopeGuard::FRDGGPUStatScopeGuard(FRDGBuilder& InGraphBuilder, const FName& Name, const FName& StatName)
	: GraphBuilder(InGraphBuilder)
{
	GraphBuilder.BeginStatScope(Name, StatName);
}

FRDGGPUStatScopeGuard::~FRDGGPUStatScopeGuard()
{
	GraphBuilder.EndStatScope();
}

static void OnPushStat(FRHIComputeCommandList& RHICmdList, const FRDGGPUStatScope* Scope)
{
#if HAS_GPU_STATS
	// GPU stats are currently only supported on the immediate command list.
	if (RHICmdList.IsImmediate())
	{
		FRealtimeGPUProfiler::Get()->PushEvent(static_cast<FRHICommandListImmediate&>(RHICmdList), Scope->Name, Scope->StatName);
	}
#endif
}

static void OnPopStat(FRHIComputeCommandList& RHICmdList)
{
#if HAS_GPU_STATS
	// GPU stats are currently only supported on the immediate command list.
	if (RHICmdList.IsImmediate())
	{
		FRealtimeGPUProfiler::Get()->PopEvent(static_cast<FRHICommandListImmediate&>(RHICmdList));
	}
#endif
}

bool FRDGGPUStatScopeStack::IsEnabled()
{
#if HAS_GPU_STATS
	return AreGPUStatsEnabled();
#else
	return false;
#endif
}

FRDGGPUStatScopeStack::FRDGGPUStatScopeStack(FRHIComputeCommandList& InRHICmdList)
	: ScopeStack(InRHICmdList, &OnPushStat, &OnPopStat)
{}

void FRDGGPUStatScopeStack::BeginScope(const FName& Name, const FName& StatName)
{
	if (IsEnabled())
	{
		ScopeStack.BeginScope(Name, StatName);
	}
}

void FRDGGPUStatScopeStack::EndScope()
{
	if (IsEnabled())
	{
		ScopeStack.EndScope();
	}
}

void FRDGGPUStatScopeStack::BeginExecute()
{
	if (IsEnabled())
	{
		ScopeStack.BeginExecute();
	}
}

void FRDGGPUStatScopeStack::BeginExecutePass(const FRDGPass* Pass)
{
	if (IsEnabled())
	{
		ScopeStack.BeginExecutePass(Pass->GetScopes().Stat);
	}
}

void FRDGGPUStatScopeStack::EndExecute()
{
	if (IsEnabled())
	{
		ScopeStack.EndExecute();
	}
}

void FRDGScopeStacksByPipeline::BeginExecutePass(const FRDGPass* Pass)
{
	ERHIPipeline Pipeline = Pass->GetPipeline();

	/**TODO(RDG): This currently crashes certain platforms. */
	if (GRDGAsyncCompute == RDG_ASYNC_COMPUTE_FORCE_ENABLED)
	{
		Pipeline = ERHIPipeline::Graphics;
	}

	GetScopeStacks(Pipeline).BeginExecutePass(Pass);
}

void FRDGScopeStacksByPipeline::EndExecutePass(const FRDGPass* Pass)
{
	ERHIPipeline Pipeline = Pass->GetPipeline();

	/**TODO(RDG): This currently crashes certain platforms. */
	if (GRDGAsyncCompute == RDG_ASYNC_COMPUTE_FORCE_ENABLED)
	{
		Pipeline = ERHIPipeline::Graphics;
	}

	GetScopeStacks(Pipeline).EndExecutePass();
}