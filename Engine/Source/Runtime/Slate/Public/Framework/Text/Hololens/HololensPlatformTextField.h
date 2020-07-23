// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IPlatformTextField.h"


class SLATE_API FHoloLensPlatformTextField : public IPlatformTextField
{
	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;
};

typedef FHoloLensPlatformTextField FPlatformTextField;
