// Copyright 2026 RainskyCG. All Rights Reserved.

#include "BlendViewCommands.h"

#include "InputCoreTypes.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "BlendViewCommands"

FBlendViewCommands::FBlendViewCommands()
	: TCommands<FBlendViewCommands>(
		TEXT("BlendView"),
		LOCTEXT("BlendViewContext", "BlendView"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FBlendViewCommands::RegisterCommands()
{
	UI_COMMAND(
		ToggleBlendView,
		"切换 BlendView",
		"启用或禁用 BlendView。",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::B, false, true, true, false));
	UI_COMMAND(
		BeginTranslate,
		"移动",
		"进入 Blender 风格移动变换。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::G));
	UI_COMMAND(
		BeginRotate,
		"旋转",
		"进入 Blender 风格旋转变换。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::R));
	UI_COMMAND(
		BeginScale,
		"缩放",
		"进入 Blender 风格缩放变换。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::S));
	UI_COMMAND(
		BeginMirror,
		"Mirror",
		"Begin Blender-style interactive mirror transform.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::M, false, true, false, false));
	UI_COMMAND(
		TogglePivotEditMode,
		"Move/Edit Pivot",
		"Toggle BlendView pivot editing during translation.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::O));
	UI_COMMAND(
		SetSnapBase,
		"设置吸附基准",
		"在 G/R/S 变换中进入吸附基准拾取状态。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::B));
	UI_COMMAND(
		ClearTransformConstraint,
		"清除变换约束",
		"清除当前 G/R/S 变换的轴或平面约束。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::C));
	UI_COMMAND(
		ToggleTransformStatusBar,
		"显示/隐藏变换提示",
		"显示或隐藏变换状态提示栏。",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::H));
	UI_COMMAND(
		OpenQuickFavorites,
		"Quick Favorites",
		"Open BlendView quick favorites for the current editor context.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Q));
	UI_COMMAND(
		OpenCommandSearch,
		"Command Search",
		"Search and execute commands for the current editor context.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::F3));
	UI_COMMAND(
		FrameSelected,
		"定位所选",
		"在鼠标所在的视口、大纲或内容浏览器中定位当前所选。",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Decimal));
	UI_COMMAND(
		DuplicateAndTranslate,
		"Duplicate and Move",
		"Duplicates the selected actors and immediately enters BlendView move transform.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::D, true, false, false, false));
	UI_COMMAND(
		MoveSelectedToFolder,
		"Move to Folder",
		"Open a Blender-style menu to move selected actors to an Outliner folder.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::M));
}

bool FBlendViewCommands::MatchesInputEvent(
	const FBlendViewInputEvent& Event,
	const FUICommandInfo& Command)
{
	const bool bIsPressEvent =
		Event.Type == EBlendViewInputEventType::KeyDown ||
		Event.Type == EBlendViewInputEventType::MouseDown;
	if (!bIsPressEvent)
	{
		return false;
	}
	if (Event.Type == EBlendViewInputEventType::KeyDown && Event.bIsRepeat)
	{
		return false;
	}

	auto MatchesChord = [&Command, &Event](const EMultipleKeyBindingIndex ChordIndex)
	{
		const FInputChord& Chord = *Command.GetActiveChord(ChordIndex);
		return Chord.IsValidChord() &&
			Event.Key == Chord.Key &&
			Event.bShiftDown == Chord.bShift &&
			Event.bControlDown == Chord.NeedsControl() &&
			Event.bAltDown == Chord.NeedsAlt() &&
			Event.bCommandDown == Chord.NeedsCommand();
	};

	return MatchesChord(EMultipleKeyBindingIndex::Primary) ||
		MatchesChord(EMultipleKeyBindingIndex::Secondary);
}

#undef LOCTEXT_NAMESPACE
