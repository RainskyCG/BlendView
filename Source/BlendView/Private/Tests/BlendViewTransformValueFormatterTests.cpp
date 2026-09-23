// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UI/BlendViewTransformValueFormatter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformValueFormatterTest,
	"BlendView.UI.TransformValueFormatter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformValueFormatterTest::RunTest(const FString& Parameters)
{
	FBlendViewTransformValueState State;
	State.Mode = EBlendViewTransformMode::Translate;
	State.Constraint = EBlendViewAxisConstraint::X;
	State.Translation = FVector(2312.0, 0.0, 0.0);
	TestEqual(
		TEXT("Single-axis English translation matches the compact Blender-style format"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("D: 23.12 m (23.12 m) along global X")));
	TestEqual(
		TEXT("Chinese translation uses compact units and an axis constraint suffix"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("D: 23.12 m (23.12 m) 沿 全局 X 轴")));

	State.Translation = FVector(937.6, 0.0, 0.0);
	TestEqual(
		TEXT("Single-digit meters retain three decimal places"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("D: 9.376 m (9.376 m) 沿 全局 X 轴")));
	State.Translation = FVector(12345.0, 0.0, 0.0);
	TestEqual(
		TEXT("Three-digit meters retain one decimal place"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("D: 123.5 m (123.5 m) 沿 全局 X 轴")));

	State.Constraint = EBlendViewAxisConstraint::None;
	State.Translation = FVector(-937.6, 259.8, -422.2);
	TestEqual(
		TEXT("Free English translation uses Dx, Dy, Dz and total distance"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Dx: -9.376 m   Dy: 2.598 m   Dz: -4.222 m (10.61 m)")));

	State.Mode = EBlendViewTransformMode::Rotate;
	State.Constraint = EBlendViewAxisConstraint::Z;
	State.RotationDegrees = -20.0;
	State.bLocalConstraint = true;
	TestEqual(
		TEXT("Rotation omits the degree symbol and includes local constraint text"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Rotation: -20.00 around local Z")));
	TestEqual(
		TEXT("Chinese rotation uses a direction constraint suffix"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("旋转: -20.00 绕 局部 Z 轴")));

	State.Constraint = EBlendViewAxisConstraint::None;
	State.bLocalConstraint = false;
	State.bTrackball = true;
	TestEqual(
		TEXT("Trackball rotation uses its own value label"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Trackball: -20.00")));

	State.Mode = EBlendViewTransformMode::Scale;
	State.bTrackball = false;
	State.Constraint = EBlendViewAxisConstraint::None;
	State.Scale = FVector(1.4, 1.4, 1.4);
	TestEqual(
		TEXT("English scale fields use two spaces"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Scale X: 1.4000  Y: 1.4000  Z: 1.4000")));
	TestEqual(
		TEXT("Chinese scale fields include the axis direction wording"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("X 向缩放: 1.4000  Y 向缩放: 1.4000  Z 向缩放: 1.4000")));

	State.Constraint = EBlendViewAxisConstraint::X;
	State.Scale = FVector(1.5, 2.0, 3.0);
	State.bLocalConstraint = false;
	TestEqual(
		TEXT("Constrained scale displays only the constrained axis value"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("全局 X 向缩放: 1.5000")));
	TestEqual(
		TEXT("Constrained English scale identifies its axis direction"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Scale along global X: 1.5000")));

	State.Mode = EBlendViewTransformMode::Translate;
	State.Constraint = EBlendViewAxisConstraint::None;
	State.Translation = FVector(-0.00001, 640.0, 0.0);
	State.bGraph = true;
	TestEqual(
		TEXT("Graph translation omits world units and cleans negative zero"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Dx: 0  Dy: 640")));

	State.Constraint = EBlendViewAxisConstraint::X;
	State.Translation = FVector(12.0, 0.0, 0.0);
	TestEqual(
		TEXT("Graph constraints omit world-space wording in English"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("D: 12 along X")));
	TestEqual(
		TEXT("Graph constraints omit world-space wording in Chinese"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("D: 12 沿 X 轴")));

	State.Mode = EBlendViewTransformMode::Rotate;
	State.RotationDegrees = 30.0;
	TestEqual(
		TEXT("Graph rotation describes rotation around an axis"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("旋转: 30.00 绕 X 轴")));

	State.Mode = EBlendViewTransformMode::Scale;
	State.Constraint = EBlendViewAxisConstraint::Y;
	State.Scale = FVector(1.0, 2.0, 1.0);
	TestEqual(
		TEXT("Graph constrained scale uses directional scale wording"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			true).ToString(),
		FString(TEXT("Y 向缩放: 2.0000")));
	State.Mode = EBlendViewTransformMode::Mirror;
	State.Constraint = EBlendViewAxisConstraint::None;
	State.bLocalConstraint = false;
	TestEqual(
		TEXT("Mirror starts by asking for an axis"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Select a mirror axis (X, Y, Z)")));

	State.Constraint = EBlendViewAxisConstraint::X;
	TestEqual(
		TEXT("Mirror single-axis constraint describes global axis direction"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Mirror along global X")));

	State.Constraint = EBlendViewAxisConstraint::PlaneX;
	State.bLocalConstraint = true;
	TestEqual(
		TEXT("Mirror locking constraint describes the locked local axis"),
		FBlendViewTransformValueFormatter::Format(
			State,
			EBlendViewTranslationNumericUnit::Meters,
			false).ToString(),
		FString(TEXT("Mirror locking local X")));
	return true;
}

#endif
