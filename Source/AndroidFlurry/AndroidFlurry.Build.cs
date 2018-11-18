// Copyright 2018 Kokku. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class AndroidFlurry : ModuleRules
	{
        public AndroidFlurry(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ApplicationCore"
				// ... add other public dependencies that you statically link with here ...
			}
            );

            PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Analytics",
				// ... add private dependencies that you statically link with here ...
			}
			);

            PublicIncludePathModuleNames.Add("Analytics");

            PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"Launch",
			}
            );

            if (Target.Platform == UnrealTargetPlatform.Android)
			{
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "AndroidFlurry_APL.xml"));
                Definitions.Add("WITH_FLURRY=1");
            }
        }
	}
}