#pragma once
#include "Modules/ModuleInterface.h"

class IRHITestModule : public IModuleInterface
{
public:
	virtual void RunAllTests() = 0;
};