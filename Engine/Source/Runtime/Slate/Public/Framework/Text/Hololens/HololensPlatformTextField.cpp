// Copyright Epic Games, Inc. All Rights Reserved.

#include "HololensPlatformTextField.h"
#include "MixedRealityInterop.h"

void FHoloLensPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) 
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
