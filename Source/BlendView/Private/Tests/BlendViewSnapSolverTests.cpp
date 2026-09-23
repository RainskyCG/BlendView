// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Snap/BlendViewSnapSolver.h"
#include "Tools/BlendViewTransformPrecision.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewSnapDeltaToGridTest,
	"BlendView.Snap.DeltaToGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewSnapDeltaToGridTest::RunTest(const FString& Parameters)
{
	const FVector Result = FBlendViewSnapSolver::SnapDeltaToGrid(FVector(149.0, -151.0, 24.0), 100.0);
	TestEqual(TEXT("Each axis snaps independently"), Result, FVector(100.0, -200.0, 0.0));
	TestEqual(
		TEXT("A disabled grid preserves the input"),
		FBlendViewSnapSolver::SnapDeltaToGrid(FVector(1.25, 2.5, 5.0), 0.0),
		FVector(1.25, 2.5, 5.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewCleanNearIntegerTest,
	"BlendView.Snap.CleanNearInteger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewCleanNearIntegerTest::RunTest(const FString& Parameters)
{
	const FVector Cleaned = FBlendViewSnapSolver::CleanNearIntegerVector(
		FVector(99.99995, -20.00005, 3.125));
	TestEqual(TEXT("Positive near-integer noise is removed"), Cleaned.X, 100.0);
	TestEqual(TEXT("Negative near-integer noise is removed"), Cleaned.Y, -20.0);
	TestEqual(TEXT("Meaningful fractions are preserved"), Cleaned.Z, 3.125);
	TestEqual(
		TEXT("Four-decimal positive noise is normalized"),
		FBlendViewTransformPrecision::CleanNearInteger(4.000031),
		4.0);
	TestEqual(
		TEXT("Four-decimal carry is normalized"),
		FBlendViewTransformPrecision::CleanNearInteger(199.99999),
		200.0);
	TestEqual(
		TEXT("Four-decimal negative noise is normalized"),
		FBlendViewTransformPrecision::CleanNearInteger(-500.000014),
		-500.0);
	TestEqual(
		TEXT("Meaningful fifth decimal is preserved"),
		FBlendViewTransformPrecision::CleanNearInteger(4.00006),
		4.00006);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewSnapCandidatePriorityTest,
	"BlendView.Snap.CandidatePriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewSnapCandidatePriorityTest::RunTest(const FString& Parameters)
{
	const EBlendViewSnapTargetKind PriorityOrder[] = {
		EBlendViewSnapTargetKind::Vertex,
		EBlendViewSnapTargetKind::EdgeMidpoint,
		EBlendViewSnapTargetKind::Edge,
		EBlendViewSnapTargetKind::Grid,
		EBlendViewSnapTargetKind::Face
	};

	for (int32 PreferredIndex = 0; PreferredIndex < UE_ARRAY_COUNT(PriorityOrder); ++PreferredIndex)
	{
		for (int32 LowerIndex = PreferredIndex + 1; LowerIndex < UE_ARRAY_COUNT(PriorityOrder); ++LowerIndex)
		{
			FBlendViewSnapCandidate Preferred;
			Preferred.Kind = PriorityOrder[PreferredIndex];
			Preferred.ScreenDistance = 100.0;

			FBlendViewSnapCandidate LowerPriority;
			LowerPriority.Kind = PriorityOrder[LowerIndex];
			LowerPriority.ScreenDistance = 1.0;

			TestTrue(
				TEXT("Target kind priority outranks screen distance"),
				FBlendViewSnapSolver::IsCandidatePreferred(Preferred, LowerPriority));
		}
	}

	FBlendViewSnapCandidate NearVertex;
	NearVertex.Kind = EBlendViewSnapTargetKind::Vertex;
	NearVertex.ScreenDistance = 5.0;
	FBlendViewSnapCandidate FarVertex = NearVertex;
	FarVertex.ScreenDistance = 10.0;
	TestTrue(
		TEXT("Within one target kind, nearest screen distance wins"),
		FBlendViewSnapSolver::IsCandidatePreferred(NearVertex, FarVertex));
	return true;
}

#endif
