// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Debugging/ConsoleSlateDebugger.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "SlateGlobals.h"
#include "Types/SlateStructs.h"


DEFINE_LOG_CATEGORY(LogSlate);
DEFINE_LOG_CATEGORY(LogSlateStyles);

const float FOptionalSize::Unspecified = -1.0f;


/**
 * Implements the SlateCore module.
 */
class FSlateCoreModule
	: public IModuleInterface
{
public:
	FSlateCoreModule()
	{
		SlateDebugger = MakeUnique<FConsoleSlateDebugger>();
	}

	TUniquePtr<FConsoleSlateDebugger> SlateDebugger;
};


IMPLEMENT_MODULE(FSlateCoreModule, SlateCore);
