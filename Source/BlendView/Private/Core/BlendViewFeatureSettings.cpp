// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Core/BlendViewFeatureSettings.h"

#include "BlendViewSettings.h"
#include "Localization/BlendViewLocalization.h"
#include "UObject/UnrealType.h"

FText FBlendViewFeatureToggleDefinition::GetDisplayName() const
{
	return FBlendViewLocalization::Text(ChineseName, EnglishName);
}

FText FBlendViewFeatureToggleDefinition::GetTooltip() const
{
	return FBlendViewLocalization::Text(ChineseTooltip, EnglishTooltip);
}

TConstArrayView<FBlendViewFeatureToggleDefinition> FBlendViewFeatureSettings::GetExperimentalFeatures()
{
	static const FBlendViewFeatureToggleDefinition Features[] = {
		{
			TEXT("SceneCursor"),
			GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableSceneCursor),
			TEXT("启用 3D 游标"),
			TEXT("Enable 3D Cursor"),
			TEXT("启用 Shift+右键放置 BlendView 3D 游标，并允许游标绘制和游标相关命令。"),
			TEXT("Enable Shift+right-click placement, drawing, and cursor-related commands for the BlendView 3D cursor."),
			TEXT("BlendViewStyle"),
			TEXT("BlendView.PivotCursor")
		},
		{
			TEXT("CursorOriginPie"),
			GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnablePieMenu),
			TEXT("启用游标与中心饼菜单"),
			TEXT("Enable Cursor and Origin Pie"),
			TEXT("按 Shift+S 在关卡视口中弹出游标与中心饼菜单。"),
			TEXT("Open the cursor and origin pie menu in the level viewport with Shift+S.")
		},
		{
			TEXT("QuickFavorites"),
			GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableQuickFavorites),
			TEXT("启用快速收藏夹"),
			TEXT("Enable Quick Favorites"),
			TEXT("按 Q 弹出 BlendView 快速收藏夹，并根据当前编辑器上下文显示可用命令。"),
			TEXT("Open BlendView quick favorites with Q and show commands for the current editor context.")
		},
		{
			TEXT("CommandSearch"),
			GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableCommandSearch),
			TEXT("启用命令搜索"),
			TEXT("Enable Command Search"),
			TEXT("按 F3 弹出当前上下文的命令搜索菜单。"),
			TEXT("Open the command search menu for the current context with F3.")
		},
		{
			TEXT("MoveToFolderMenu"),
			GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableMoveToFolderMenu),
			TEXT("启用移动到文件夹菜单"),
			TEXT("Enable Move to Folder Menu"),
			TEXT("按 M 在关卡视口中弹出移动到文件夹菜单。"),
			TEXT("Open the move-to-folder menu in the level viewport with M.")
		}
	};
	return Features;
}

const FBlendViewFeatureToggleDefinition* FBlendViewFeatureSettings::FindExperimentalFeature(const FName PropertyName)
{
	for (const FBlendViewFeatureToggleDefinition& Feature : GetExperimentalFeatures())
	{
		if (Feature.PropertyName == PropertyName)
		{
			return &Feature;
		}
	}
	return nullptr;
}

bool FBlendViewFeatureSettings::IsEnabled(const UBlendViewSettings* Settings, const FName PropertyName)
{
	if (!Settings)
	{
		return false;
	}

	const FBoolProperty* Property = FindBoolProperty(PropertyName);
	return Property && Property->GetPropertyValue_InContainer(Settings);
}

bool FBlendViewFeatureSettings::Toggle(UBlendViewSettings* Settings, const FName PropertyName)
{
	if (!Settings)
	{
		return false;
	}

	FBoolProperty* Property = FindBoolProperty(PropertyName);
	if (!Property)
	{
		return false;
	}

	const bool bNewValue = !Property->GetPropertyValue_InContainer(Settings);
	Property->SetPropertyValue_InContainer(Settings, bNewValue);
	Settings->SaveConfig();
	return bNewValue;
}

FBoolProperty* FBlendViewFeatureSettings::FindBoolProperty(const FName PropertyName)
{
	return FindFProperty<FBoolProperty>(UBlendViewSettings::StaticClass(), PropertyName);
}