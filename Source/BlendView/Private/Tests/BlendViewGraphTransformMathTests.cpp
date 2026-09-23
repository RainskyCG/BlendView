// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Graph/BlendViewGraphTransformMath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewGraphTransformPivotTest,
	"BlendView.Graph.Transform.Pivot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewGraphTransformPivotTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2f> Positions =
	{
		FVector2f(-20.0f, 10.0f),
		FVector2f(100.0f, 70.0f),
		FVector2f(40.0f, 20.0f)
	};
	TestTrue(
		TEXT("Pivot uses the bounds of node position points"),
		FBlendViewGraphTransformMath::CalculatePivot(Positions).Equals(
			FVector2f(40.0f, 40.0f),
			KINDA_SMALL_NUMBER));

	const TArray<FVector2f> SinglePosition = {FVector2f(125.0f, -30.0f)};
	TestTrue(
		TEXT("A single node rotates and scales around its own position"),
		FBlendViewGraphTransformMath::CalculatePivot(SinglePosition).Equals(
			SinglePosition[0],
			KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewGraphTransformMathTest,
	"BlendView.Graph.Transform.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewGraphTransformMathTest::RunTest(const FString& Parameters)
{
	const FVector2f Pivot(10.0f, 20.0f);
	const FVector2f Position(30.0f, 20.0f);
	TestTrue(
		TEXT("Positive graph rotation follows screen-space clockwise orientation"),
		FBlendViewGraphTransformMath::RotatePosition(Position, Pivot, UE_HALF_PI).Equals(
			FVector2f(10.0f, 40.0f),
			0.001f));

	TestTrue(
		TEXT("X constrained scale preserves Y"),
		FBlendViewGraphTransformMath::ScalePosition(
			FVector2f(30.0f, 40.0f),
			Pivot,
			2.0,
			EBlendViewGraphConstraint::X).Equals(
			FVector2f(50.0f, 40.0f),
			0.001f));

	TestTrue(
		TEXT("Y constrained translation removes X motion"),
		FBlendViewGraphTransformMath::ConstrainTranslation(
			FVector2f(25.0f, -15.0f),
			EBlendViewGraphConstraint::Y).Equals(
			FVector2f(0.0f, -15.0f),
			0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewGraphNumericDirectionTest,
	"BlendView.Graph.Transform.NumericDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewGraphNumericDirectionTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Free numeric move follows the current pointer intent"),
		FBlendViewGraphTransformMath::GetNumericTranslationDirection(
			FVector2f(3.0f, 4.0f),
			EBlendViewGraphConstraint::None).Equals(
			FVector2f(0.6f, 0.8f),
			0.001f));
	TestTrue(
		TEXT("X constraint defines numeric direction"),
		FBlendViewGraphTransformMath::GetNumericTranslationDirection(
			FVector2f(-3.0f, 4.0f),
			EBlendViewGraphConstraint::X).Equals(
			FVector2f(1.0f, 0.0f),
			0.001f));
	TestTrue(
		TEXT("Numeric move defaults to positive X before pointer movement"),
		FBlendViewGraphTransformMath::GetNumericTranslationDirection(
			FVector2f::ZeroVector,
			EBlendViewGraphConstraint::None).Equals(
			FVector2f(1.0f, 0.0f),
			0.001f));
	return true;
}

#endif
