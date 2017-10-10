// Copyright 2016 Chris Conway (Koderz). All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RuntimeMeshComponent : ModuleRules
{
    private string ModulePath
    {
        get { return ModuleDirectory; }
    }

    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/")); }
    }

    public RuntimeMeshComponent(ReadOnlyTargetRules rules): base(rules)
	{
        PrivateIncludePaths.Add("RuntimeMeshComponent/Private");
        PublicIncludePaths.Add("RuntimeMeshComponent/Public");

        PublicDependencyModuleNames.AddRange(
                new string[]
                {
                        "Core",
                        "CoreUObject",
                        "Engine",
                        "RenderCore",
                        "ShaderCore",
                        "RHI"
                }
            );
        LoadElaraSDK(rules);
    }

    public void LoadElaraSDK(ReadOnlyTargetRules rules)
    {
     if (rules.Platform == UnrealTargetPlatform.Win64)
        {
            string LibraryPath = Path.Combine(ThirdPartyPath, "ERSDK", "lib");
            PublicLibraryPaths.Add(LibraryPath);
            PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "liber.lib"));
            PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "ERSDK", "include"));
        }
    }
    
 
}

