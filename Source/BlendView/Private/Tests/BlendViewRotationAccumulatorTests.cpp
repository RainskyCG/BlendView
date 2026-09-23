// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Tools/BlendViewRotationAccumulator.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewRotationAccumulatorMultiTurnTest,
	"BlendView.Transform.RotationAccumulator.MultiTurn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewRotationAccumulatorMultiTurnTest::RunTest(const FString& Parameters)
{
	FBlendViewRotationAccumulator Accumulator;
	TestTrue(TEXT("Initial direction is accepted"), Accumulator.Begin(FVector::XAxisVector));

	double AngleRadians = 0.0;
	const FVector SignedViewNormal = FVector::ZAxisVector;
	const FVector Directions[] =
	{
		FVector::YAxisVector,
		-FVector::XAxisVector,
		-FVector::YAxisVector,
		FVector::XAxisVector,
		FVector::YAxisVector
	};

	for (const FVector& Direction : Directions)
	{
		TestTrue(
			TEXT("Each incremental direction is accepted"),
			Accumulator.Evaluate(Direction, SignedViewNormal, AngleRadians));
	}

	TestTrue(
		TEXT("Accumulation continues beyond one full turn"),
		FMath::IsNearlyEqual(AngleRadians, -2.5 * UE_PI, KINDA_SMALL_NUMBER));
	return true;
}

#endif
