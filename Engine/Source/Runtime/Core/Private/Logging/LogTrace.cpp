// Copyright Epic Games, Inc. All Rights Reserved.
#include "Logging/LogTrace.h"

#if LOGTRACE_ENABLED

#include "Trace/Trace.inl"
#include "Templates/Function.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformTLS.h"

UE_TRACE_CHANNEL(LogChannel)

UE_TRACE_EVENT_BEGIN(Logging, LogCategory, NoSync|Important)
	UE_TRACE_EVENT_FIELD(const void*, CategoryPointer)
	UE_TRACE_EVENT_FIELD(uint8, DefaultVerbosity)
	UE_TRACE_EVENT_FIELD(Trace::AnsiString, Name)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Logging, LogMessageSpec, NoSync|Important)
	UE_TRACE_EVENT_FIELD(const void*, LogPoint)
	UE_TRACE_EVENT_FIELD(const void*, CategoryPointer)
	UE_TRACE_EVENT_FIELD(int32, Line)
	UE_TRACE_EVENT_FIELD(uint8, Verbosity)
	UE_TRACE_EVENT_FIELD(Trace::AnsiString, File)
	UE_TRACE_EVENT_FIELD(Trace::WideString, FormatString)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Logging, LogMessage)
	UE_TRACE_EVENT_FIELD(const void*, LogPoint)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
UE_TRACE_EVENT_END()

void FLogTrace::OutputLogCategory(const FLogCategoryBase* Category, const TCHAR* Name, ELogVerbosity::Type DefaultVerbosity)
{
	uint16 NameSize = uint16(FCString::Strlen(Name));
	UE_TRACE_LOG(Logging, LogCategory, LogChannel)
		<< LogCategory.CategoryPointer(Category)
		<< LogCategory.DefaultVerbosity(DefaultVerbosity)
		<< LogCategory.Name(Name, NameSize);
}

void FLogTrace::OutputLogMessageSpec(const void* LogPoint, const FLogCategoryBase* Category, ELogVerbosity::Type Verbosity, const ANSICHAR* File, int32 Line, const TCHAR* Format)
{
	uint16 FileNameSize = uint16(strlen(File));
	uint16 FormatStringSize = uint16(FCString::Strlen(Format));
	UE_TRACE_LOG(Logging, LogMessageSpec, LogChannel)
		<< LogMessageSpec.LogPoint(LogPoint)
		<< LogMessageSpec.CategoryPointer(Category)
		<< LogMessageSpec.Line(Line)
		<< LogMessageSpec.Verbosity(Verbosity)
		<< LogMessageSpec.File(File, FileNameSize)
		<< LogMessageSpec.FormatString(Format, FormatStringSize);
}

void FLogTrace::OutputLogMessageInternal(const void* LogPoint, uint16 EncodedFormatArgsSize, uint8* EncodedFormatArgs)
{
	UE_TRACE_LOG(Logging, LogMessage, LogChannel, EncodedFormatArgsSize)
		<< LogMessage.LogPoint(LogPoint)
		<< LogMessage.Cycle(FPlatformTime::Cycles64())
		<< LogMessage.Attachment(EncodedFormatArgs, EncodedFormatArgsSize);
}

#endif
