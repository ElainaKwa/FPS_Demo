// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyFps_Demo : ModuleRules
{
	public MyFps_Demo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTasks",
			"GameplayTags",
			"UMG",
			"AIModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnLua",
			"Lua"
		});

		PublicIncludePaths.AddRange(new string[] {
			"MyFps_Demo",
			"MyFps_Demo/Weapons",
			"MyFps_Demo/GameAbilitySystem",
			"MyFps_Demo/GameAbilitySystem/Abilities/Fire",
			"MyFps_Demo/GameAbilitySystem/Abilities/Reload"
		});
	}
}
