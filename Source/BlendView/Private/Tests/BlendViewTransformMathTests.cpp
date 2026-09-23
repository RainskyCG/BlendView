// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "Tools/BlendViewPlaneTranslationSession.h"
#include "Tools/BlendViewTransformMath.h"
#include "Tools/BlendViewTransformPrecision.h"
#include "Tools/BlendViewVirtualPointerTracker.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double TransformTolerance = 0.001;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewVirtualPointerUnboundedTest,
	"BlendView.Transform.VirtualPointer.Unbounded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewVirtualPointerUnboundedTest::RunTest(const FString& Parameters)
{
	FBlendViewVirtualPointerTracker Pointer;
	Pointer.Reset(FVector2D(1915.0, 1075.0));
	const FVector2D Result = Pointer.Advance(FVector2D(2500.0, -1800.0), 1.0);
	TestTrue(
		TEXT("Virtual pointer remains unbounded beyond the viewport"),
		Result.Equals(FVector2D(4415.0, -725.0), TransformTolerance));

	Pointer.Reset(FVector2D(100.0, 100.0));
	const FVector2D PrecisionResult = Pointer.Advance(FVector2D(20.0, -10.0), 0.1);
	TestTrue(
		TEXT("Precision mode scales delta without clamping the pointer"),
		PrecisionResult.Equals(FVector2D(102.0, 99.0), TransformTolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewPlaneTranslationContinuityTest,
	"BlendView.Transform.PlaneTranslation.Continuity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewPlaneTranslationContinuityTest::RunTest(const FString& Parameters)
{
	FBlendViewPlaneTranslationSession Session;
	const FVector ExistingDelta(120.0, -40.0, 75.0);
	const FVector PointerAnchor(1000.0, 500.0, 0.0);
	const FVector EntryDelta = Session.Begin(
		PointerAnchor,
		ExistingDelta,
		FQuat::Identity,
		2);

	TestTrue(
		TEXT("Entering projects the current movement onto the plane without using the pointer as an absolute target"),
		EntryDelta.Equals(FVector(120.0, -40.0, 0.0), TransformTolerance));
	TestTrue(
		TEXT("The pointer anchor produces no additional movement at entry"),
		Session.Solve(PointerAnchor).Equals(EntryDelta, TransformTolerance));
	TestTrue(
		TEXT("Only pointer movement after entering the constraint changes position"),
		Session.Solve(PointerAnchor + FVector(300.0, -200.0, 0.0)).Equals(
			FVector(420.0, -240.0, 0.0),
			TransformTolerance));
	TestTrue(
		TEXT("Reapplying the plane constraint keeps the frozen axis at its entry value"),
		Session.ConstrainDelta(FVector(800.0, 900.0, -500.0)).Equals(
			FVector(800.0, 900.0, 0.0),
			TransformTolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewLocalPlaneTranslationTest,
	"BlendView.Transform.PlaneTranslation.LocalSpace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewLocalPlaneTranslationTest::RunTest(const FString& Parameters)
{
	const FQuat LocalRotation(FVector::UpVector, FMath::DegreesToRadians(90.0));
	FBlendViewPlaneTranslationSession Session;
	Session.Begin(
		FVector::ZeroVector,
		FVector::ZeroVector,
		LocalRotation,
		0);

	const FVector Result = Session.Solve(FVector(10.0, 20.0, 30.0));
	const FVector LocalResult = LocalRotation.UnrotateVector(Result);
	TestEqual(TEXT("Excluded local axis remains fixed"), LocalResult.X, 0.0);
	TestTrue(TEXT("Movement remains available on the local plane"), !Result.IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewRotateAroundPivotTest,
	"BlendView.Transform.RotateAroundPivot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewRotateAroundPivotTest::RunTest(const FString& Parameters)
{
	const FTransform Baseline(FQuat::Identity, FVector(200.0, 100.0, 0.0), FVector::OneVector);
	const FVector Pivot(100.0, 100.0, 0.0);
	const FQuat QuarterTurn(FVector::UpVector, FMath::DegreesToRadians(90.0));

	const FTransform Result = FBlendViewTransformMath::BuildRotatedTransform(Baseline, Pivot, QuarterTurn);
	TestTrue(TEXT("Location rotates around the pivot"), Result.GetLocation().Equals(FVector(100.0, 200.0, 0.0), TransformTolerance));
	TestTrue(TEXT("Rotation receives the same delta"), Result.GetRotation().Equals(QuarterTurn, TransformTolerance));
	TestTrue(TEXT("Scale remains unchanged"), Result.GetScale3D().Equals(FVector::OneVector, TransformTolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewRotateAroundIndividualOriginTest,
	"BlendView.Transform.RotateAroundIndividualOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewRotateAroundIndividualOriginTest::RunTest(const FString& Parameters)
{
	const FTransform Baseline(FQuat::Identity, FVector(200.0, 100.0, 0.0), FVector::OneVector);
	const FQuat QuarterTurn(FVector::UpVector, FMath::DegreesToRadians(90.0));

	const FTransform Result = FBlendViewTransformMath::BuildRotatedTransform(
		Baseline,
		Baseline.GetLocation(),
		QuarterTurn);
	TestTrue(TEXT("Individual-origin rotation keeps object location fixed"), Result.GetLocation().Equals(Baseline.GetLocation(), TransformTolerance));
	TestTrue(TEXT("Individual-origin rotation still applies the rotation delta"), Result.GetRotation().Equals(QuarterTurn, TransformTolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewFinalRotationPrecisionTest,
	"BlendView.Transform.FinalRotationPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewFinalRotationPrecisionTest::RunTest(const FString& Parameters)
{
	const FTransform Baseline(
		FRotator(0.0, 179.999992, 0.0).Quaternion(),
		FVector::ZeroVector,
		FVector::OneVector);

	const FTransform Result = FBlendViewTransformMath::BuildRotatedTransform(
		Baseline,
		FVector::ZeroVector,
		FQuat::Identity);
	const FRotator FinalRotation = Result.GetRotation().Rotator();

	TestEqual(TEXT("Final yaw removes near-integer quaternion conversion residue"), FinalRotation.Yaw, 180.0);
	TestEqual(TEXT("Final pitch remains exact"), FinalRotation.Pitch, 0.0);
	TestEqual(TEXT("Final roll remains exact"), FinalRotation.Roll, 0.0);

	const FRotator CleanQuarterTurn = FBlendViewTransformPrecision::CleanNearInteger(
		FRotator(0.0, 0.0, -89.999992));
	TestEqual(TEXT("Negative quarter-turn roll is exact at the property boundary"), CleanQuarterTurn.Roll, -90.0);
	TestEqual(TEXT("Quarter-turn pitch remains exact"), CleanQuarterTurn.Pitch, 0.0);
	TestEqual(TEXT("Quarter-turn yaw remains exact"), CleanQuarterTurn.Yaw, 0.0);

	USceneComponent* Component = NewObject<USceneComponent>(GetTransientPackage());
	Component->SetRelativeTransform(FTransform(
		FRotator(0.0, 0.0, -90.0).Quaternion(),
		FVector::ZeroVector,
		FVector::OneVector));
	Component->SetRelativeRotationExact(
		FBlendViewTransformPrecision::CleanNearInteger(Component->GetRelativeRotation()));
	TestEqual(
		TEXT("Exact component writeback preserves the cleaned quarter-turn property"),
		Component->GetRelativeRotation().Roll,
		-90.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewWorldScaleAroundPivotTest,
	"BlendView.Transform.WorldScaleAroundPivot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewWorldScaleAroundPivotTest::RunTest(const FString& Parameters)
{
	const FTransform Baseline(FQuat::Identity, FVector(20.0, 30.0, 40.0), FVector(2.0, 3.0, 4.0));
	const FVector Pivot(10.0, 10.0, 10.0);

	const FTransform Result = FBlendViewTransformMath::BuildScaledTransform(
		Baseline,
		Pivot,
		FVector(2.0, 0.5, -1.0),
		FQuat::Identity,
		false);
	TestTrue(TEXT("Location scales around the pivot"), Result.GetLocation().Equals(FVector(30.0, 20.0, -20.0), TransformTolerance));
	TestTrue(TEXT("Object scale preserves signed axis factors"), Result.GetScale3D().Equals(FVector(4.0, 1.5, -4.0), TransformTolerance));
	TestTrue(TEXT("Rotation remains unchanged"), Result.GetRotation().Equals(FQuat::Identity, TransformTolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewScaleAroundIndividualOriginTest,
	"BlendView.Transform.ScaleAroundIndividualOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewScaleAroundIndividualOriginTest::RunTest(const FString& Parameters)
{
	const FTransform Baseline(FQuat::Identity, FVector(20.0, 30.0, 40.0), FVector(2.0, 3.0, 4.0));

	const FTransform Result = FBlendViewTransformMath::BuildScaledTransform(
		Baseline,
		Baseline.GetLocation(),
		FVector(2.0, 0.5, 1.5),
		FQuat::Identity,
		false);
	TestTrue(TEXT("Individual-origin scale keeps object location fixed"), Result.GetLocation().Equals(Baseline.GetLocation(), TransformTolerance));
	TestTrue(TEXT("Individual-origin scale still applies object scale"), Result.GetScale3D().Equals(FVector(4.0, 1.5, 6.0), TransformTolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewFinalScalePrecisionTest,
	"BlendView.Transform.FinalScalePrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewFinalScalePrecisionTest::RunTest(const FString& Parameters)
{
	const FTransform Baseline(
		FQuat::Identity,
		FVector::ZeroVector,
		FVector(1.00000025, 1.0000002, 0.5));

	const FTransform Result = FBlendViewTransformMath::BuildScaledTransform(
		Baseline,
		FVector::ZeroVector,
		FVector(8.0),
		FQuat::Identity,
		false);

	TestEqual(TEXT("Final X scale removes near-integer multiplication residue"), Result.GetScale3D().X, 8.0);
	TestEqual(TEXT("Final Y scale removes near-integer multiplication residue"), Result.GetScale3D().Y, 8.0);
	TestEqual(TEXT("Final Z scale remains exact"), Result.GetScale3D().Z, 4.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewLocalConstraintScaleTest,
	"BlendView.Transform.LocalConstraintScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewLocalConstraintScaleTest::RunTest(const FString& Parameters)
{
	const FQuat ConstraintRotation(FVector::UpVector, FMath::DegreesToRadians(90.0));
	const FTransform Baseline(FQuat::Identity, FVector(0.0, 10.0, 0.0), FVector::OneVector);

	const FTransform Result = FBlendViewTransformMath::BuildScaledTransform(
		Baseline,
		FVector::ZeroVector,
		FVector(2.0, 1.0, 1.0),
		ConstraintRotation,
		true);
	TestTrue(TEXT("Location uses the rotated local scale space"), Result.GetLocation().Equals(FVector(0.0, 20.0, 0.0), TransformTolerance));
	TestTrue(TEXT("Scale factors are projected onto object axes"), Result.GetScale3D().Equals(FVector(1.0, 2.0, 1.0), TransformTolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewMirrorReflectsRotationTest,
	"BlendView.Transform.MirrorReflectsRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewMirrorReflectsRotationTest::RunTest(const FString& Parameters)
{
	const FTransform Baseline(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(45.0)),
		FVector(10.0, 20.0, 0.0),
		FVector::OneVector);

	const FTransform Result = FBlendViewTransformMath::BuildMirroredTransform(
		Baseline,
		FVector::ZeroVector,
		FVector(-1.0, 1.0, 1.0),
		FQuat::Identity,
		false);

	TestTrue(TEXT("Location is reflected in mirror space"), Result.GetLocation().Equals(FVector(-10.0, 20.0, 0.0), TransformTolerance));
	TestTrue(TEXT("Mirror keeps the signed local scale factor"), Result.GetScale3D().Equals(FVector(-1.0, 1.0, 1.0), TransformTolerance));
	TestTrue(TEXT("Rotation is reflected instead of left unchanged"), FMath::IsNearlyEqual(Result.Rotator().Yaw, -45.0, TransformTolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewMirrorLockingKeepsBlenderScaleSignsTest,
	"BlendView.Transform.MirrorLockingKeepsBlenderScaleSigns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewMirrorLockingKeepsBlenderScaleSignsTest::RunTest(const FString& Parameters)
{
	const FTransform Baseline(FQuat::Identity, FVector(10.0, 20.0, 30.0), FVector::OneVector);

	const FTransform Result = FBlendViewTransformMath::BuildMirroredTransform(
		Baseline,
		FVector::ZeroVector,
		FVector(1.0, -1.0, -1.0),
		FQuat::Identity,
		false);

	TestTrue(TEXT("Locking X reflects the Y/Z location components"), Result.GetLocation().Equals(FVector(10.0, -20.0, -30.0), TransformTolerance));
	TestTrue(TEXT("Locking X keeps Blender's Y/Z negative scale signs"), Result.GetScale3D().Equals(FVector(1.0, -1.0, -1.0), TransformTolerance));
	TestTrue(TEXT("Identity rotation remains identity for locking X"), Result.GetRotation().Equals(FQuat::Identity, TransformTolerance));
	return true;
}

#endif
