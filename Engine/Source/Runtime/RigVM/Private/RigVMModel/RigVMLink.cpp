// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMGraph.h"

URigVMGraph* URigVMLink::GetGraph()
{
	return Cast<URigVMGraph>(GetOuter());
}

void URigVMLink::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		Ar << SourcePinPath;
		Ar << TargetPinPath;
	}
	else
	{
		FString SourcePinPathTemp = SourcePin->GetPinPath();
		FString TargetPinPathTemp = TargetPin->GetPinPath();
		Ar << SourcePinPathTemp;
		Ar << TargetPinPathTemp;
	}
}

URigVMPin* URigVMLink::GetSourcePin()
{
	if (SourcePin == nullptr)
	{
		SourcePin = GetGraph()->FindPin(SourcePinPath);
		if (SourcePin)
		{
			SourcePinPath.Empty();
		}
	}
	return SourcePin;
}

URigVMPin* URigVMLink::GetTargetPin()
{
	if (TargetPin == nullptr)
	{
		TargetPin = GetGraph()->FindPin(TargetPinPath);
		if (TargetPin)
		{
			TargetPinPath.Empty();
		}
	}
	return TargetPin;
}

FString URigVMLink::GetPinPathRepresentation()
{
	return FString::Printf(TEXT("%s -> %s"), *GetSourcePin()->GetPinPath(), *GetTargetPin()->GetPinPath());
}
