// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class XRVisualization : ModuleRules
	{
		public XRVisualization(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("Runtime/XR/XRVisualization/Private");
			PublicIncludePaths.Add("Runtime/XR/XRVisualization/Public");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
                    "EngineSettings",
                    "RenderCore",
					"RHI",
				}
			);
        }
	}
}
