// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "BlendViewCommands.h"
#include "Tools/BlendViewTransformStatusBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformStatusConstraintHintTest,
	"BlendView.UI.TransformStatus.ConstraintHint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformStatusConstraintHintTest::RunTest(const FString& Parameters)
{
	const FInputChord& ClearConstraintChord =
		*FBlendViewCommands::Get().ClearTransformConstraint->GetActiveChord(
			EMultipleKeyBindingIndex::Primary);
	const FString ClearConstraintText = ClearConstraintChord.GetInputText().ToString();
	auto HasClearConstraintToken = [&ClearConstraintText](const FBlendViewStatusLine& Line)
	{
		for (const FBlendViewStatusHint& Hint : Line.Hints)
		{
			for (const FBlendViewStatusToken& Token : Hint.Tokens)
			{
				if (Token.Text.ToString() == ClearConstraintText)
				{
					return true;
				}
			}
		}
		return false;
	};

	FBlendViewTransformStatusState State;
	State.Constraint = EBlendViewAxisConstraint::None;
	TestFalse(
		TEXT("Unconstrained transforms omit the clear-constraint hint"),
		HasClearConstraintToken(FBlendViewTransformStatusBuilder::Build(State)));

	State.Constraint = EBlendViewAxisConstraint::X;
	const FBlendViewStatusLine ConstrainedLine = FBlendViewTransformStatusBuilder::Build(State);
	TestTrue(
		TEXT("Constrained transforms expose the configured clear-constraint shortcut"),
		HasClearConstraintToken(ConstrainedLine));
	TestTrue(
		TEXT("Transform status starts with the cancel operation"),
		ConstrainedLine.Hints.Num() > 0 &&
			ConstrainedLine.Hints[0].Tokens.Num() == 1 &&
			ConstrainedLine.Hints[0].Tokens[0].Kind == EBlendViewStatusTokenKind::Mouse &&
			ConstrainedLine.Hints[0].Tokens[0].Text.ToString() == TEXT("RMB"));
	TestTrue(
		TEXT("Transform status starts with cancel before confirm"),
		ConstrainedLine.Hints.Num() > 1 &&
			ConstrainedLine.Hints[1].Tokens.Num() == 1 &&
			ConstrainedLine.Hints[1].Tokens[0].Text.ToString() == TEXT("LMB"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformStatusGraphProfileTest,
	"BlendView.UI.TransformStatus.GraphProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformStatusGraphProfileTest::RunTest(const FString& Parameters)
{
	auto HasToken = [](const FBlendViewStatusLine& Line, const TCHAR* Value)
	{
		for (const FBlendViewStatusHint& Hint : Line.Hints)
		{
			for (const FBlendViewStatusToken& Token : Hint.Tokens)
			{
				if (Token.Text.ToString() == Value)
				{
					return true;
				}
			}
		}
		return false;
	};

	FBlendViewTransformStatusShortcuts Shortcuts;
	Shortcuts.Translate = FText::FromString(TEXT("G"));
	Shortcuts.Rotate = FText::FromString(TEXT("R"));
	Shortcuts.Scale = FText::FromString(TEXT("S"));
	Shortcuts.PivotEditMode = FText::FromString(TEXT("O"));
	Shortcuts.ClearConstraint = FText::FromString(TEXT("C"));
	Shortcuts.SetSnapBase = FText::FromString(TEXT("B"));
	Shortcuts.ToggleHints = FText::FromString(TEXT("H"));

	FBlendViewTransformStatusState State;
	State.Mode = EBlendViewTransformMode::Translate;
	State.bSupportsZAxis = false;
	State.bSupportsPlaneConstraint = false;
	State.bSupportsAutoConstraint = true;
	State.bSupportsAutoConstraintPlane = false;
	State.bSupportsTrackball = false;
	State.bSupportsSnapBase = false;
	State.bSupportsPivotEdit = false;
	State.bUseResizeLabel = true;

	const FBlendViewStatusLine TranslateLine =
		FBlendViewTransformStatusBuilder::Build(State, Shortcuts);
	TestFalse(TEXT("Graph profile omits the unsupported Z axis"), HasToken(TranslateLine, TEXT("Z")));
	TestFalse(TEXT("Graph profile omits snap-base controls"), HasToken(TranslateLine, TEXT("B")));
	TestFalse(TEXT("Graph profile omits pivot-edit controls"), HasToken(TranslateLine, TEXT("O")));
	TestTrue(TEXT("Graph translation exposes automatic constraint"), HasToken(TranslateLine, TEXT("MMB")));

	State.Mode = EBlendViewTransformMode::Mirror;
	State.bSupportsZAxis = true;
	State.bSupportsPlaneConstraint = true;
	State.bSupportsAutoConstraint = true;
	State.bSupportsAutoConstraintPlane = true;
	const FBlendViewStatusLine MirrorLine =
		FBlendViewTransformStatusBuilder::Build(State, Shortcuts);
	TestTrue(TEXT("Mirror exposes automatic constraint"), HasToken(MirrorLine, TEXT("MMB")));
	TestTrue(TEXT("Mirror exposes automatic constraint plane modifier"), HasToken(MirrorLine, TEXT("Shift")));

	State.Mode = EBlendViewTransformMode::Rotate;
	State.bSupportsAutoConstraint = false;
	State.bSupportsAutoConstraintPlane = false;
	const FBlendViewStatusLine RotateLine =
		FBlendViewTransformStatusBuilder::Build(State, Shortcuts);
	TestFalse(TEXT("Graph rotation omits automatic constraint"), HasToken(RotateLine, TEXT("MMB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformStatusPivotHintOrderTest,
	"BlendView.UI.TransformStatus.PivotHintOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformStatusPivotHintOrderTest::RunTest(const FString& Parameters)
{
	FBlendViewTransformStatusShortcuts Shortcuts;
	Shortcuts.Translate = FText::FromString(TEXT("G"));
	Shortcuts.Rotate = FText::FromString(TEXT("R"));
	Shortcuts.Scale = FText::FromString(TEXT("S"));
	Shortcuts.PivotEditMode = FText::FromString(TEXT("O"));
	Shortcuts.ClearConstraint = FText::FromString(TEXT("C"));
	Shortcuts.SetSnapBase = FText::FromString(TEXT("B"));
	Shortcuts.ToggleHints = FText::FromString(TEXT("H"));

	FBlendViewTransformStatusState State;
	State.Mode = EBlendViewTransformMode::Translate;
	const FBlendViewStatusLine Line = FBlendViewTransformStatusBuilder::Build(State, Shortcuts);
	TestTrue(TEXT("Translate status has at least pivot and hide hints"), Line.Hints.Num() >= 2);
	const FBlendViewStatusHint& PivotHint = Line.Hints[Line.Hints.Num() - 2];
	const FBlendViewStatusHint& HideHint = Line.Hints.Last();
	TestTrue(
		TEXT("Pivot hint is the penultimate hint"),
		PivotHint.Tokens.Num() == 1 &&
			PivotHint.Tokens[0].Text.ToString() == TEXT("O"));
	TestTrue(
		TEXT("Hide hint remains last"),
		HideHint.Tokens.Num() == 1 &&
			HideHint.Tokens[0].Text.ToString() == TEXT("H"));
	return true;
}

#endif
