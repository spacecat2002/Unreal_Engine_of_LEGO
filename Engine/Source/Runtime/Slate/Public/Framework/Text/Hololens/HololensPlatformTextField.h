// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IPlatformTextField.h"

#include "MixedRealityInterop.h"

class FHoloLensPlatformTextField : public IPlatformTextField
{
	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override
	{
		if (bShow)
		{
			WindowsMixedReality::MixedRealityInterop::ShowSystemKeyboard();
		}
		else
		{
			WindowsMixedReality::MixedRealityInterop::HideSystemKeyboard();
		}
	}
};

typedef FHoloLensPlatformTextField FPlatformTextField;
