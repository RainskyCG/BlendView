// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewTransformStatusBuilder.h"

#include "BlendViewCommands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Localization/BlendViewLocalization.h"

namespace
{
	FText Localized(const TCHAR* Chinese, const TCHAR* English)
	{
		return FBlendViewLocalization::Text(Chinese, English);
	}

	FText Raw(const TCHAR* Value)
	{
		return FText::FromString(Value);
	}

	FBlendViewStatusToken Key(const FText& Value)
	{
		return {EBlendViewStatusTokenKind::Key, Value};
	}

	FBlendViewStatusToken Mouse(const TCHAR* Value)
	{
		return {EBlendViewStatusTokenKind::Mouse, Raw(Value)};
	}

	FText Chord(const TSharedPtr<FUICommandInfo>& Command, const TCHAR* Fallback)
	{
		if (Command.IsValid())
		{
			const FInputChord& InputChord = *Command->GetActiveChord(EMultipleKeyBindingIndex::Primary);
			if (InputChord.IsValidChord())
			{
				return InputChord.GetInputText();
			}
		}
		return Raw(Fallback);
	}

	FBlendViewStatusHint& AddHint(
		FBlendViewStatusLine& Line,
		const TCHAR* Chinese,
		const TCHAR* English)
	{
		FBlendViewStatusHint& Hint = Line.Hints.AddDefaulted_GetRef();
		Hint.Label = Localized(Chinese, English);
		return Hint;
	}

	void AddKeyHint(
		FBlendViewStatusLine& Line,
		const TCHAR* Chinese,
		const TCHAR* English,
		const TCHAR* Value)
	{
		AddHint(Line, Chinese, English).Tokens.Add(Key(Raw(Value)));
	}

	void AddPivotEditHint(
		FBlendViewStatusLine& Line,
		const FBlendViewTransformStatusState& State,
		const FBlendViewTransformStatusShortcuts& Shortcuts)
	{
		if (State.Mode != EBlendViewTransformMode::Translate || !State.bSupportsPivotEdit)
		{
			return;
		}
		if (State.bPivotEditMode)
		{
			AddHint(Line, TEXT("编辑枢轴点"), TEXT("Edit Pivot")).Tokens.Add(Key(Shortcuts.PivotEditMode));
		}
		else if (!State.bModelingPivotEditMode)
		{
			AddHint(Line, TEXT("移动枢轴点"), TEXT("Move Pivot")).Tokens.Add(Key(Shortcuts.PivotEditMode));
		}
	}
}

FBlendViewStatusLine FBlendViewTransformStatusBuilder::Build(
	const FBlendViewTransformStatusState& State)
{
	const FBlendViewCommands& Commands = FBlendViewCommands::Get();
	FBlendViewTransformStatusShortcuts Shortcuts;
	Shortcuts.Translate = Chord(Commands.BeginTranslate, TEXT("G"));
	Shortcuts.Rotate = Chord(Commands.BeginRotate, TEXT("R"));
	Shortcuts.Scale = Chord(Commands.BeginScale, TEXT("S"));
	Shortcuts.Mirror = Chord(Commands.BeginMirror, TEXT("Ctrl+M"));
	Shortcuts.PivotEditMode = Chord(Commands.TogglePivotEditMode, TEXT("O"));
	Shortcuts.ClearConstraint = Chord(Commands.ClearTransformConstraint, TEXT("C"));
	Shortcuts.SetSnapBase = Chord(Commands.SetSnapBase, TEXT("B"));
	Shortcuts.ToggleHints = Chord(Commands.ToggleTransformStatusBar, TEXT("H"));
	return Build(State, Shortcuts);
}

FBlendViewStatusLine FBlendViewTransformStatusBuilder::Build(
	const FBlendViewTransformStatusState& State,
	const FBlendViewTransformStatusShortcuts& Shortcuts)
{
	FBlendViewStatusLine Line;
	if (State.bSelectingSnapBase)
	{
		AddHint(Line, TEXT("确认基准"), TEXT("Confirm Base")).Tokens.Add(Mouse(TEXT("LMB")));
		AddHint(Line, TEXT("取消基准"), TEXT("Cancel Base")).Tokens.Add(Mouse(TEXT("RMB")));
		AddHint(Line, TEXT("结束设置基准"), TEXT("Finish Set Base")).Tokens.Add(
			Key(Shortcuts.SetSnapBase));
		return Line;
	}

	AddHint(Line, TEXT("取消"), TEXT("Cancel")).Tokens.Add(Mouse(TEXT("RMB")));
	AddHint(Line, TEXT("确认"), TEXT("Confirm")).Tokens.Add(Mouse(TEXT("LMB")));
	{
		FBlendViewStatusHint& Hint = AddHint(Line, TEXT("轴向"), TEXT("Axis"));
		Hint.Tokens = {Key(Raw(TEXT("X"))), Key(Raw(TEXT("Y")))};
		if (State.bSupportsZAxis)
		{
			Hint.Tokens.Add(Key(Raw(TEXT("Z"))));
		}
	}
	if (State.Mode == EBlendViewTransformMode::Mirror)
	{
		if (State.Constraint != EBlendViewAxisConstraint::None)
		{
			AddHint(Line, TEXT("清除约束"), TEXT("Clear Constraint")).Tokens.Add(
				Key(Shortcuts.ClearConstraint));
		}
		if (State.bSupportsPlaneConstraint)
		{
			FBlendViewStatusHint& Hint = AddHint(Line, TEXT("锁定"), TEXT("Locking"));
			Hint.Tokens = {Key(Raw(TEXT("Shift"))), Key(Raw(TEXT("X"))), Key(Raw(TEXT("Y"))), Key(Raw(TEXT("Z")))};
		}
		if (State.bSupportsAutoConstraint)
		{
			AddHint(Line, TEXT("自动约束"), TEXT("Auto Constraint")).Tokens.Add(Mouse(TEXT("MMB")));
		}
		if (State.bSupportsAutoConstraintPlane)
		{
			FBlendViewStatusHint& Hint = AddHint(Line, TEXT("自动约束平面"), TEXT("Auto Constraint Plane"));
			Hint.Tokens = {Key(Raw(TEXT("Shift"))), Mouse(TEXT("MMB"))};
		}
		AddHint(Line, TEXT("隐藏提示"), TEXT("Hide Hints")).Tokens.Add(
			Key(Shortcuts.ToggleHints));
		return Line;
	}
	if (State.Constraint != EBlendViewAxisConstraint::None)
	{
		AddHint(Line, TEXT("清除约束"), TEXT("Clear Constraint")).Tokens.Add(
			Key(Shortcuts.ClearConstraint));
	}
	AddKeyHint(Line, TEXT("吸附反转"), TEXT("Invert Snap"), TEXT("Ctrl"));
	if (State.bSupportsPlaneConstraint)
	{
		FBlendViewStatusHint& Hint = AddHint(Line, TEXT("平面"), TEXT("Plane"));
		Hint.Tokens = {Key(Raw(TEXT("Shift"))), Key(Raw(TEXT("X"))), Key(Raw(TEXT("Y"))), Key(Raw(TEXT("Z")))};
	}
	if (State.Mode != EBlendViewTransformMode::Translate)
	{
		AddHint(Line, TEXT("移动"), TEXT("Move")).Tokens.Add(Key(Shortcuts.Translate));
	}
	if (State.Mode != EBlendViewTransformMode::Rotate)
	{
		AddHint(Line, TEXT("旋转"), TEXT("Rotate")).Tokens.Add(Key(Shortcuts.Rotate));
	}
	if (State.Mode != EBlendViewTransformMode::Scale)
	{
		AddHint(
			Line,
			State.bUseResizeLabel ? TEXT("调整大小") : TEXT("缩放"),
			State.bUseResizeLabel ? TEXT("Resize") : TEXT("Scale")).Tokens.Add(Key(Shortcuts.Scale));
	}
	if (State.bSupportsAutoConstraint)
	{
		AddHint(Line, TEXT("自动约束"), TEXT("Auto Constraint")).Tokens.Add(Mouse(TEXT("MMB")));
	}
	if (State.bSupportsAutoConstraintPlane)
	{
		FBlendViewStatusHint& Hint = AddHint(Line, TEXT("自动约束平面"), TEXT("Auto Constraint Plane"));
		Hint.Tokens = {Key(Raw(TEXT("Shift"))), Mouse(TEXT("MMB"))};
	}
	AddKeyHint(Line, TEXT("精确模式"), TEXT("Precision"), TEXT("Shift"));
	if (State.NumericBuffer.IsSet())
	{
		AddKeyHint(Line, TEXT("删除数值"), TEXT("Delete Value"), TEXT("Backspace"));
	}
	if (State.bSupportsTrackball &&
		State.Mode == EBlendViewTransformMode::Rotate &&
		!State.bFreeRotate)
	{
		AddHint(Line, TEXT("轨迹球"), TEXT("Trackball")).Tokens.Add(Key(Shortcuts.Rotate));
	}
	if (State.bSupportsSnapBase)
	{
		AddHint(
			Line,
			State.bHasSnapBase ? TEXT("重设吸附基准") : TEXT("设置吸附基准"),
			State.bHasSnapBase ? TEXT("Reset Snap Base") : TEXT("Set Snap Base")).Tokens.Add(
			Key(Shortcuts.SetSnapBase));
	}
	AddPivotEditHint(Line, State, Shortcuts);

	AddHint(Line, TEXT("隐藏提示"), TEXT("Hide Hints")).Tokens.Add(
		Key(Shortcuts.ToggleHints));
	return Line;
}
