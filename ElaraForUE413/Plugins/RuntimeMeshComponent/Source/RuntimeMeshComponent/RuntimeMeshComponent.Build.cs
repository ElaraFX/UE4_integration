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

    public RuntimeMeshComponent(TargetInfo rules)
	{
        PrivateIncludePaths.Add("RuntimeMeshComponent/Private");
        string ue4_include_path = Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, "Source/Runtime/Engine/Private"));
        PublicIncludePaths.Add(ue4_include_path);
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
        if (UEBuildConfiguration.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",
                    "MaterialEditor"
                });

        }
        LoadElaraSDK(rules);
    }

    public void LoadElaraSDK(TargetInfo rules)
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

