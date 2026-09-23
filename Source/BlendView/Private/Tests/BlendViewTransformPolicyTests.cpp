// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Tools/BlendViewTransformConstraintUtils.h"
#include "Tools/BlendViewTransformResetPolicy.h"
#include "Tools/BlendViewTransformSnapSession.h"
#include "Tools/BlendViewScaleSpring.h"
#include "Tools/BlendViewTransformSnapPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformSnapSessionTest,
	"BlendView.Transform.Policy.SnapSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformSnapSessionTest::RunTest(const FString& Parameters)
{
	FBlendViewTransformSnapSession Session;
	Session.SetActiveTarget(
		FVector(1.0, 2.0, 3.0),
		FVector::UpVector,
		true,
		EBlendViewSnapTargetKind::Face);
	TestTrue(TEXT("Active target is stored"), Session.bHasActiveTarget);
	TestTrue(TEXT("Active normal is stored"), Session.bHasActiveTargetNormal);
	TestEqual(TEXT("Active target kind is stored"), Session.ActiveTargetKind, EBlendViewSnapTargetKind::Face);

	Session.ClearActiveTarget();
	TestFalse(TEXT("Active target clears"), Session.bHasActiveTarget);
	TestFalse(TEXT("Active normal clears"), Session.bHasActiveTargetNormal);
	TestEqual(TEXT("Active target kind resets"), Session.ActiveTargetKind, EBlendViewSnapTargetKind::None);

	Session.BeginBaseSelection();
	TestTrue(TEXT("Base selection starts"), Session.bSelectingBase);
	Session.SetBaseCandidate(FVector(10.0, 20.0, 30.0), EBlendViewSnapTargetKind::Vertex);
	Session.ConfirmBaseSelection();
	TestFalse(TEXT("Base selection exits after confirm"), Session.bSelectingBase);
	TestTrue(TEXT("Base is stored after confirm"), Session.bHasBase);
	TestTrue(TEXT("Base location is stored"), Session.BaseLocation.Equals(FVector(10.0, 20.0, 30.0)));

	Session.BeginBaseSelection();
	Session.SetBaseCandidate(FVector(40.0, 50.0, 60.0), EBlendViewSnapTargetKind::Edge);
	Session.CancelBaseSelection();
	TestFalse(TEXT("Base selection exits after cancel"), Session.bSelectingBase);
	TestFalse(TEXT("Candidate clears after cancel"), Session.bHasBaseCandidate);
	TestTrue(TEXT("Confirmed base survives cancel"), Session.BaseLocation.Equals(FVector(10.0, 20.0, 30.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformResetPolicyTest,
	"BlendView.Transform.Policy.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformResetPolicyTest::RunTest(const FString& Parameters)
{
	const TOptional<EBlendViewTransformMode> LocationMode =
		FBlendViewTransformResetPolicy::GetFallbackMode(EBlendViewTransformResetChannel::Location);
	const TOptional<EBlendViewTransformMode> RotationMode =
		FBlendViewTransformResetPolicy::GetFallbackMode(EBlendViewTransformResetChannel::Rotation);
	const TOptional<EBlendViewTransformMode> ScaleMode =
		FBlendViewTransformResetPolicy::GetFallbackMode(EBlendViewTransformResetChannel::Scale);
	TestTrue(TEXT("Location reset has fallback"), LocationMode.IsSet());
	TestTrue(TEXT("Rotation reset has fallback"), RotationMode.IsSet());
	TestTrue(TEXT("Scale reset has fallback"), ScaleMode.IsSet());
	TestEqual(
		TEXT("Location reset falls back to translate"),
		LocationMode.GetValue(),
		EBlendViewTransformMode::Translate);
	TestEqual(
		TEXT("Rotation reset falls back to rotate"),
		RotationMode.GetValue(),
		EBlendViewTransformMode::Rotate);
	TestEqual(
		TEXT("Scale reset falls back to scale"),
		ScaleMode.GetValue(),
		EBlendViewTransformMode::Scale);
	TestTrue(
		TEXT("Translate mode accepts location reset"),
		FBlendViewTransformResetPolicy::DoesChannelMatchMode(
			EBlendViewTransformResetChannel::Location,
			EBlendViewTransformMode::Translate));
	TestFalse(
		TEXT("Translate mode consumes but does not apply rotation reset"),
		FBlendViewTransformResetPolicy::DoesChannelMatchMode(
			EBlendViewTransformResetChannel::Rotation,
			EBlendViewTransformMode::Translate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformConstraintPolicyTest,
	"BlendView.Transform.Policy.Constraints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformConstraintPolicyTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("X maps to the YZ plane"),
		FBlendViewTransformConstraintUtils::AxisToPlane(EBlendViewAxisConstraint::X),
		EBlendViewAxisConstraint::PlaneX);
	TestEqual(
		TEXT("XY plane excludes Z"),
		FBlendViewTransformConstraintUtils::GetPlaneExcludedAxis(EBlendViewAxisConstraint::PlaneZ),
		EBlendViewAxisConstraint::Z);
	TestTrue(
		TEXT("Y is active on the YZ plane"),
		FBlendViewTransformConstraintUtils::IsAxisActive(
			EBlendViewAxisConstraint::PlaneX,
			EBlendViewAxisConstraint::Y));
	TestFalse(
		TEXT("X is excluded from the YZ plane"),
		FBlendViewTransformConstraintUtils::IsAxisActive(
			EBlendViewAxisConstraint::PlaneX,
			EBlendViewAxisConstraint::X));
	TestTrue(
		TEXT("Plane projection removes the excluded axis"),
		FBlendViewTransformConstraintUtils::ProjectVectorOntoPlane(
			FVector(10.0, 20.0, 30.0),
			EBlendViewAxisConstraint::PlaneX,
			FQuat::Identity).Equals(FVector(0.0, 20.0, 30.0)));
	TestTrue(
		TEXT("No constraint preserves the vector"),
		FBlendViewTransformConstraintUtils::ApplyVectorConstraint(
			FVector(10.0, 20.0, 30.0),
			EBlendViewAxisConstraint::None,
			1.0,
			false,
			FQuat::Identity).Equals(FVector(10.0, 20.0, 30.0)));
	TestTrue(
		TEXT("Axis constraint projects onto the active axis"),
		FBlendViewTransformConstraintUtils::ApplyVectorConstraint(
			FVector(10.0, 20.0, 30.0),
			EBlendViewAxisConstraint::Y,
			1.0,
			false,
			FQuat::Identity).Equals(FVector(0.0, 20.0, 0.0)));

	const FQuat QuarterTurn(FVector::UpVector, FMath::DegreesToRadians(90.0));
	const FVector LocalX = FBlendViewTransformConstraintUtils::GetAxisVector(
		EBlendViewAxisConstraint::X,
		true,
		QuarterTurn);
	TestTrue(TEXT("Local X follows constraint rotation"), LocalX.Equals(FVector::YAxisVector, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformSnapPolicyTest,
	"BlendView.Transform.Policy.Snap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformSnapPolicyTest::RunTest(const FString& Parameters)
{
	const FVector AxisSnapped = FBlendViewTransformSnapPolicy::SnapTranslationDelta(
		FVector(149.0, 75.0, 0.0),
		100.0,
		EBlendViewAxisConstraint::X,
		FQuat::Identity);
	TestTrue(TEXT("Axis snap removes orthogonal motion"), AxisSnapped.Equals(FVector(100.0, 0.0, 0.0)));

	const FVector PlaneScale = FBlendViewTransformSnapPolicy::BuildScaleFactorVector(
		2.0,
		EBlendViewAxisConstraint::PlaneX);
	TestTrue(TEXT("Plane scale preserves excluded axis"), PlaneScale.Equals(FVector(1.0, 2.0, 2.0)));

	const FVector MirrorLockingXScale = FBlendViewTransformSnapPolicy::BuildScaleFactorVector(
		-1.0,
		EBlendViewAxisConstraint::PlaneX);
	TestTrue(TEXT("Mirror locking X flips Y and Z"), MirrorLockingXScale.Equals(FVector(1.0, -1.0, -1.0)));

	const double Rotation = FBlendViewTransformSnapPolicy::SnapRotationRadians(
		FMath::DegreesToRadians(23.0),
		true,
		false,
		EBlendViewAxisConstraint::Z,
		FRotator(10.0, 15.0, 20.0));
	TestTrue(
		TEXT("Rotation uses the selected axis grid"),
		FMath::IsNearlyEqual(FMath::RadiansToDegrees(Rotation), 30.0));

	FBlendViewScaleSpring ScaleSpring;
	ScaleSpring.Begin(FVector2D::ZeroVector, FVector2D(100.0, 0.0));
	double ScaleFactor = 0.0;
	TestTrue(TEXT("Scale spring evaluates an active session"), ScaleSpring.Evaluate(FVector2D(200.0, 0.0), ScaleFactor));
	TestTrue(TEXT("Scale spring preserves radial ratio"), FMath::IsNearlyEqual(ScaleFactor, 2.0));
	TestTrue(TEXT("Scale spring supports crossing the pivot"), ScaleSpring.Evaluate(FVector2D(-50.0, 0.0), ScaleFactor));
	TestTrue(TEXT("Crossing the pivot produces negative scale"), FMath::IsNearlyEqual(ScaleFactor, -0.5));
	return true;
}

#endif
