// Copyright Epic Games, Inc. All Rights Reserved.
#include "HAL/LowLevelMemTracker.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

#include "LowLevelMemoryUtils.h"
#include "Misc/AutomationTest.h"


LLM_DECLARE_TAG(TEXT("LLMTestChildTag1"), NAME_None, TEXT("LLMTestParentTag")); // A test tag to verify that child tags can be declared before parent tags
LLM_DECLARE_TAG(TEXT("LLMTestParentTag"), NAME_None, NAME_None);
LLM_DECLARE_TAG(TEXT("LLMTestChildTag2"), NAME_None, TEXT("LLMTestParentTag")); // A test tag to verify that child tags can be declared after parent tags


#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTopologicalSortTest, "System.Core.LLM.TopologicalSort", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTopologicalSortTest::RunTest(const FString& Parameters)
{
	TArray<TArray<int32>> Edges;
	auto AppendEdges = [&Edges](int32 InVertex, int32* OutEdges, int32& OutNumEdges)
	{
		TArray<int32>& VertexEdges = Edges[InVertex];
		OutNumEdges = VertexEdges.Num();
		if (OutNumEdges)
		{
			FMemory::Memcpy(OutEdges, VertexEdges.GetData(), OutNumEdges*sizeof(VertexEdges[0]));
		}
	};

	// Simple case:
	//							0
	//						|		|
	//						v		v
	//						1  ->	2
	{
		TArray<TArray<int32>> RootToLeafEdges;
		RootToLeafEdges.Add(TArray<int32>{ 1, 2});
		RootToLeafEdges.Add(TArray<int32>{ 2 });
		RootToLeafEdges.AddDefaulted();

		Edges = RootToLeafEdges;
		LLMAlgo::TopologicalSortRootToLeaf(Edges, AppendEdges);
		TestTrue(TEXT("SimpleCase pre-sorted RootToLeaf remains stable sorted"), Edges == RootToLeafEdges);

		TArray<TArray<int32>> ReverseEdges;
		for (int n = RootToLeafEdges.Num() - 1; n >= 0; --n)
		{
			ReverseEdges.Add(RootToLeafEdges[n]);
		}
		LLMAlgo::TopologicalSortLeafToRoot(Edges, AppendEdges);
		TestTrue(TEXT("SimpleCase reverse-sorted RootToLeaf sorts into reverse order"), Edges == ReverseEdges);
		
		TArray<TArray<int32>> LeafToRootEdges;
		LeafToRootEdges.AddDefaulted();
		LeafToRootEdges.Add(TArray<int32>{ 0});
		LeafToRootEdges.Add(TArray<int32>{ 1,0 });
		Edges = LeafToRootEdges;
		LLMAlgo::TopologicalSortLeafToRoot(Edges, AppendEdges);
		TestTrue(TEXT("SimpleCase pre-sorted LeafToRoof remains stable sorted"), Edges == LeafToRootEdges);

		ReverseEdges.Reset();
		for (int n = LeafToRootEdges.Num() - 1; n >= 0; --n)
		{
			ReverseEdges.Add(LeafToRootEdges[n]);
		}
		LLMAlgo::TopologicalSortRootToLeaf(Edges, AppendEdges);
		TestTrue(TEXT("SimpleCase reverse-sorted LeafToRoof sorts into reverse order"), Edges == ReverseEdges);
	}

	// Simple cycle
	//							0
	//						|		|
	//						v		v
	//						1	->	2
	//							<-	|
	//								v
	//								3								
	{
		TArray<TArray<int32>> CycleEdges;
		CycleEdges.Add(TArray<int32>{ 1, 2});
		CycleEdges.Add(TArray<int32>{ 2 });
		CycleEdges.Add(TArray<int32>{ 1,3 });
		CycleEdges.AddDefaulted();

		Edges = CycleEdges;
		LLMAlgo::TopologicalSortLeafToRoot(Edges, AppendEdges);
		TestTrue(TEXT("Cycle correctly sorts target of cycle first in LeafToRoot order"), Edges[0] == CycleEdges[3]);
		TestTrue(TEXT("Cycle correctly sorts referencer of cycle last in LeafToRoot order"), Edges[3] == CycleEdges[0]);
		TestTrue(TEXT("Cycle correctly sorts vertex1 of cycle at an unspecified location into the middle"), Edges[1] == CycleEdges[1] || Edges[1] == CycleEdges[2]);
		TestTrue(TEXT("Cycle correctly sorts vertex2 of cycle at an unspecified location into the middle"), (Edges[2] == CycleEdges[1] || Edges[2] == CycleEdges[2]) && Edges[2] != Edges[1]);
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS

#endif // ENABLE_LOW_LEVEL_MEM_TRACKER