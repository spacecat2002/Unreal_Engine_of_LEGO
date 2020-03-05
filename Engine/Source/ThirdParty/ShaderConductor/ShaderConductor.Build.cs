// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class ShaderConductor : ModuleRules
{
	public ShaderConductor(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Target.UEThirdPartySourceDirectory + "ShaderConductor/ShaderConductor/Include");

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string[] DynamicLibrariesMac = new string[] {
				"/libdxcompiler.dylib",
                "/libShaderConductor.dylib",
			};

			string BinariesDir = Target.UEThirdPartyBinariesDirectory + "../Mac";
            foreach (string Lib in DynamicLibrariesMac)
			{
				string LibraryPath = BinariesDir + Lib;
				PublicDelayLoadDLLs.Add(LibraryPath);
				RuntimeDependencies.Add(LibraryPath);
				PublicAdditionalLibraries.Add(LibraryPath);
			}
			RuntimeDependencies.Add(BinariesDir + "/libdxcompiler.3.7.dylib");
		}
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			string LibPath = Target.UEThirdPartySourceDirectory + "ShaderConductor/ShaderConductor/lib/Win64/ShaderConductor.lib";
			PublicAdditionalLibraries.Add(LibPath);

			string SCBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/ShaderConductor/Win64/");
			AddDependency(SCBinariesDir, "dxcompiler_sc.dll", false);
			AddDependency(SCBinariesDir, "ShaderConductor.dll", true);
		}
		else
		{
			string Err = string.Format("Attempt to build against ShaderConductor on unsupported platform {0}", Target.Platform);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}
	}

	private void AddDependency(string SCBinariesDir, string InDLL, bool bAddPDB)
	{
		string FileName = Path.Combine(SCBinariesDir, InDLL);
		RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
		if (bAddPDB)
		{
			RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
		}
		PublicDelayLoadDLLs.Add(InDLL);
	}
}
