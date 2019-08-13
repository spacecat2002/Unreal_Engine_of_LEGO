// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMNotifications.h"

class URigVMGraph;
class URigVMNode;
class URigVMPin;

class RIGVM_API FRigVMGraphUtils
{
public:

	FRigVMGraphUtils();
	FRigVMGraphUtils(URigVMGraph* InGraph, URigVMPin* InCycleCheckingPin = nullptr, bool InCycleCheckingPinIsInput = true);
	~FRigVMGraphUtils();

	URigVMGraph* GetGraph() { return Graph;  }
	URigVMPin* GetCycleCheckPin() { return LastCycleCheckingPin; }
	bool GetCycleCheckPinIsInput() { return LastCycleCheckingPinIsInput; }

	void Reset();

	// returns the distance for a given node to the output node farthest away
	int32 GetMaxDistanceToLeafOutput(URigVMNode* InNode) const;

	// returns true if the graph can be sorted and stores the order of execution in the order array
	// return false if there's a cycle in the graph and stores the cycle in the potentialcycle array
	bool TopologicalSort(TArray<URigVMNode*>& OutOrder, TArray<URigVMNode*>& OutPotentialCycle);

	// Finds the first cycle as a node index. Returns an empty array if there's no cycle
	TArray<URigVMNode*> FindCycle();

	void PrepareCycleChecking(URigVMPin* InCycleCheckingPin = nullptr, bool InCycleCheckingPinIsInput = true);
	bool IsNodeOnCycle(URigVMNode* InNode);

private:

	URigVMGraph* Graph;
	URigVMPin* LastCycleCheckingPin;
	bool LastCycleCheckingPinIsInput;

	bool IsNodeCyclic(int32 NodeIndex);

	FDelegateHandle ModifiedHandle;
	TSet<int32> CycleWhiteList;
	TSet<int32> CycleGrayList;
	TSet<int32> CycleBlackList;
	TMap<int32, int32> CycleDepthTraversal;
	TArray<URigVMNode*> Cycle;
	TArray<bool> NodeIsOnCycle;
};
