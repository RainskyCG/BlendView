// Copyright 2026 RainskyCG. All Rights Reserved.

using UnrealBuildTool;

public class BlendView : ModuleRules
{
	public BlendView(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"BlueprintGraph",
				"DeveloperSettings",
				"Engine",
				"GraphEditor",
				"InputCore",
				"InteractiveToolsFramework",
				"Kismet",
				"LevelEditor",
				"MaterialEditor",
				"PropertyEditor",
				"Projects",
				"SceneOutliner",
				"Settings",
				"Slate",
				"SlateCore",
				"StatusBar",
				"SubobjectDataInterface",
				"SubobjectEditor",
				"ToolMenus",
				"UnrealEd"
			});
	}
}
