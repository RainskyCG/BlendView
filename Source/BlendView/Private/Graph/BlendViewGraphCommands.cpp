// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Graph/BlendViewGraphCommands.h"

#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "BlendViewGraphCommands"

FBlendViewGraphCommands::FBlendViewGraphCommands()
	: TCommands<FBlendViewGraphCommands>(
		TEXT("BlendViewGraph"),
		LOCTEXT("BlendViewGraphContext", "BlendView Graph"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FBlendViewGraphCommands::RegisterCommands()
{
	UI_COMMAND(
		BeginTranslate,
		"移动节点",
		"进入 Blender 风格的图表节点移动。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::G));
	UI_COMMAND(
		BeginRotate,
		"旋转节点布局",
		"围绕选中节点中心旋转节点布局，节点控件本身保持正向。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::R));
	UI_COMMAND(
		BeginScale,
		"缩放节点布局",
		"围绕选中节点中心缩放节点间距，节点控件尺寸保持不变。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::S));
	UI_COMMAND(
		DuplicateAndTranslate,
		"Duplicate and Move Nodes",
		"Duplicates selected graph nodes and immediately enters BlendView move transform.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::D, true, false, false, false));
	UI_COMMAND(
		DeleteAndReconnect,
		"Delete and Reconnect Nodes",
		"Deletes selected Blueprint graph nodes and reconnects exec pins using Unreal's delete-and-reconnect semantics.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::X, false, true, false, false));
}

#undef LOCTEXT_NAMESPACE
