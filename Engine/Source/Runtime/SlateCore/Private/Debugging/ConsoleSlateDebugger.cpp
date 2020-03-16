// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConsoleSlateDebugger.h"

#if WITH_SLATE_DEBUGGING

#include "Debugging/SlateDebugging.h"
#include "Widgets/SBoxPanel.h"
#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/PlatformStackWalk.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlateDebugger, Log, All);

#define LOCTEXT_NAMESPACE "ConsoleSlateDebugger"

FConsoleSlateDebugger::FConsoleSlateDebugger()
	: bCaptureStack(0)
	, EnabledInputEvents(false, (uint8)ESlateDebuggingInputEvent::MAX)
	, EnabledFocusEvents(false, (uint8)ESlateDebuggingFocusEvent::MAX)
	, StartDebuggingCommand(
		TEXT("SlateDebugger.Start"),
		*LOCTEXT("StartDebugger", "Starts the debugger.").ToString(),
		FConsoleCommandDelegate::CreateRaw(this, &FConsoleSlateDebugger::StartDebugging))
	, StopDebuggingCommand(
		TEXT("SlateDebugger.Stop"),
		*LOCTEXT("StopDebugger", "Stops the debugger.").ToString(),
		FConsoleCommandDelegate::CreateRaw(this, &FConsoleSlateDebugger::StopDebugging))
	, CaptureStackVariable(
		TEXT("SlateDebugger.CaptureStack"),
		bCaptureStack,
		*LOCTEXT("CaptureStack", "Should we capture the stack when there are events?").ToString())
	, SetInputFilterCommand(
		TEXT("SlateDebugger.SetInputFilter"),
		*LOCTEXT("SetInputFilter", "Enable or Disable specific input filters").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FConsoleSlateDebugger::SetInputFilter))
	, SetFocusFilterCommand(
		TEXT("SlateDebugger.SetFocusFilter"),
		*LOCTEXT("SetFocusFilter", "Enable or Disable specific focus filters").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FConsoleSlateDebugger::SetFocusFilter))
{
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseMove] = true;
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseEnter] = true;
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseLeave] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseButtonDown] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseButtonUp] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseButtonDoubleClick] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseWheel] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::TouchStart] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::TouchEnd] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragDetected] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragEnter] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragLeave] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragOver] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragDrop] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DropMessage] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::KeyDown] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::KeyUp] = true;
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::KeyChar] = true;
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::AnalogInput] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::TouchGesture] = true;

	EnabledFocusEvents[(uint8)ESlateDebuggingFocusEvent::FocusChanging] = true;
	//EnabledFocusEvents[(uint8)ESlateDebuggingFocusEvent::FocusLost] = true;
	//EnabledFocusEvents[(uint8)ESlateDebuggingFocusEvent::FocusReceived] = true;
}

FConsoleSlateDebugger::~FConsoleSlateDebugger()
{
	RemoveListeners();
}

void FConsoleSlateDebugger::StartDebugging()
{
	UE_LOG(LogSlateDebugger, Log, TEXT("Start Slate Debugger"));

	UpdateListeners();
}

void FConsoleSlateDebugger::StopDebugging()
{
	UE_LOG(LogSlateDebugger, Log, TEXT("Stop Slate Debugger"));

	RemoveListeners();
}

namespace SlateDebuggerUtils
{
	void SetEventFilter(const TArray< FString >& Params, const UEnum* EventEnum, TBitArray<>& Bitfield)
	{
		if (Params.Num() != 2)
		{
			return;
		}

		const int64 InputEventEnumValue = EventEnum->GetValueByNameString(Params[0]);
		// We are casting ESlateDebuggingInputEvent::MAX to uint8. Let's make sure it has a valid value. Prefer to crash now than doing bad memory access.
		check(EventEnum->GetMaxEnumValue() < (int64)TNumericLimits<uint8>::Max());
		if (InputEventEnumValue == INDEX_NONE)
		{
			return;
		}

		bool bEnable = false;
		if (!LexTryParseString(bEnable, *Params[1]))
		{
			return;
		}

		Bitfield[InputEventEnumValue] = bEnable;
	}
}

void FConsoleSlateDebugger::SetInputFilter(const TArray< FString >& Params)
{
	SlateDebuggerUtils::SetEventFilter(Params, StaticEnum<ESlateDebuggingInputEvent>(), EnabledInputEvents);
}

void FConsoleSlateDebugger::SetFocusFilter(const TArray< FString >& Params)
{
	SlateDebuggerUtils::SetEventFilter(Params, StaticEnum<ESlateDebuggingFocusEvent>(), EnabledFocusEvents);
}

void FConsoleSlateDebugger::RemoveListeners()
{
	FSlateDebugging::Warning.RemoveAll(this);
	FSlateDebugging::InputEvent.RemoveAll(this);
	FSlateDebugging::FocusEvent.RemoveAll(this);
	FSlateDebugging::AttemptNavigationEvent.RemoveAll(this);
	FSlateDebugging::MouseCaptureEvent.RemoveAll(this);
}

void FConsoleSlateDebugger::UpdateListeners()
{
	RemoveListeners();

	FSlateDebugging::Warning.AddRaw(this, &FConsoleSlateDebugger::OnWarning);
	FSlateDebugging::InputEvent.AddRaw(this, &FConsoleSlateDebugger::OnInputEvent);
	FSlateDebugging::FocusEvent.AddRaw(this, &FConsoleSlateDebugger::OnFocusEvent);
	FSlateDebugging::AttemptNavigationEvent.AddRaw(this, &FConsoleSlateDebugger::OnAttemptNavigationEvent);
	FSlateDebugging::ExecuteNavigationEvent.AddRaw(this, &FConsoleSlateDebugger::OnExecuteNavigationEvent);
	FSlateDebugging::MouseCaptureEvent.AddRaw(this, &FConsoleSlateDebugger::OnCaptureStateChangeEvent);
}

void FConsoleSlateDebugger::OnWarning(const FSlateDebuggingWarningEventArgs& EventArgs)
{
	UE_LOG(LogSlateDebugger, Warning, TEXT("%s"), *EventArgs.ToText().ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnInputEvent(const FSlateDebuggingInputEventArgs& EventArgs)
{
	// If the input event isn't in the set we care about don't write it out.
	if (!EnabledInputEvents[(uint8)EventArgs.InputEventType])
	{
		return;
	}

	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *EventArgs.ToText().ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnFocusEvent(const FSlateDebuggingFocusEventArgs& EventArgs)
{
	if (!EnabledFocusEvents[(uint8)EventArgs.FocusEventType])
	{
		return;
	}

	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *EventArgs.ToText().ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnAttemptNavigationEvent(const FSlateDebuggingNavigationEventArgs& EventArgs)
{
	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *EventArgs.ToText().ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnExecuteNavigationEvent(const FSlateDebuggingExecuteNavigationEventArgs& EventArgs)
{
	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnCaptureStateChangeEvent(const FSlateDebuggingMouseCaptureEventArgs& EventArgs)
{
	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *EventArgs.ToText().ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OptionallyDumpCallStack()
{
	if (!bCaptureStack)
	{
		return;
	}

	PrintScriptCallstack();

	TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(7, 5);

	for (int i = 0; i < Stack.Num(); i++)
	{
		UE_LOG(LogSlateDebugger, Log, TEXT("%s"), ANSI_TO_TCHAR(Stack[i].FunctionName));

		//ANSICHAR HumanReadableString[1024];
		//if (FPlatformStackWalk::SymbolInfoToHumanReadableString(Stack[i], HumanReadableString, 1024))
		//{
		//	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), ANSI_TO_TCHAR((const ANSICHAR*)&HumanReadableString[0]));
		//}
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_SLATE_DEBUGGING
