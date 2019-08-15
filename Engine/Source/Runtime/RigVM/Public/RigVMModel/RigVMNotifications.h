// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RigVMNotifications.generated.h"

class URigVMGraph;

UENUM(BlueprintType)
enum class ERigVMGraphNotifType : uint8
{
	GraphChanged,
	NodeAdded,
	NodeRemoved,
	NodeSelected,
	NodeDeselected,
	NodePositionChanged,
	NodeSizeChanged,
	PinArraySizeChanged,
	PinDefaultValueChanged,
	PinDirectionChanged,
	PinTypeChanged,
	LinkAdded,
	LinkRemoved,
	CommentTextChanged,
	RerouteCompactnessChanged,
	VariableRenamed,
	ParameterRenamed,
	Invalid
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FRigVMGraphModifiedEvent, ERigVMGraphNotifType /* type */, URigVMGraph* /* graph */, UObject* /* subject */);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRigVMGraphModifiedDynamicEvent, ERigVMGraphNotifType, NotifType, URigVMGraph*, Graph, UObject*, Subject);