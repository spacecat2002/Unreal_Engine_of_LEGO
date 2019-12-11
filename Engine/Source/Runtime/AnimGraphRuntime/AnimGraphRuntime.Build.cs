// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AnimGraphRuntime : ModuleRules
{
	public AnimGraphRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/AnimGraphRuntime/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject", 
				"Engine",
                "AnimationCore",
			}
		);

        SetupModulePhysicsSupport(Target);

		// External users of this library do not need to know about Eigen.
        AddEngineThirdPartyPrivateStaticDependencies(Target,
                "Eigen"
                );

        if (Target.bCompileChaos || Target.bUseChaos)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
					"ChaosSolvers",
					"GeometryCollectionEngine",
					"GeometryCollectionSimulationCore"
                }
            );
        }
    }
}
