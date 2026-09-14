// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project48 : ModuleRules
{
	public Project48(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks", "UMG", "Slate", "SlateCore", "OnlineSubsystem", "OnlineSubsystemUtils", "PCG" });

		PrivateDependencyModuleNames.AddRange(new string[] { "SQLiteCore", "GameplayMessageRuntime", "HTTP", "Json" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
