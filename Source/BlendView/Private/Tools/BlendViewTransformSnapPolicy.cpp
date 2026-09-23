// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewTransformSnapPolicy.h"

#include "Snap/BlendViewSnapSolver.h"
#include "Tools/BlendViewTransformConstraintUtils.h"

FVector FBlendViewTransformSnapPolicy::SnapTranslationDelta(
	const FVector& Delta,
	const double GridSize,
	const EBlendViewAxisConstraint Constraint,
	const FQuat& ConstraintRotation)
{
	if (GridSize <= UE_SMALL_NUMBER)
	{
		return Delta;
	}
	if (FBlendViewTransformConstraintUtils::IsSingleAxis(Constraint))
	{
		const FVector Axis = FBlendViewTransformConstraintUtils::GetAxisVector(
			Constraint,
			true,
			ConstraintRotation);
		const double Distance = FVector::DotProduct(Delta, Axis);
		return Axis * FBlendViewSnapSolver::SnapScalarToGrid(Distance, GridSize);
	}
	if (FBlendViewTransformConstraintUtils::IsPlane(Constraint))
	{
		const FVector LocalDelta = FBlendViewTransformConstraintUtils::RemovePlaneExcludedComponent(
			ConstraintRotation.UnrotateVector(Delta),
			Constraint);
		return ConstraintRotation.RotateVector(
			FBlendViewSnapSolver::SnapDeltaToGrid(LocalDelta, GridSize));
	}
	return FBlendViewSnapSolver::SnapDeltaToGrid(Delta, GridSize);
}

double FBlendViewTransformSnapPolicy::SnapRotationRadians(
	const double AngleRadians,
	const bool bIncrementSnap,
	const bool bEditorSnap,
	const EBlendViewAxisConstraint Constraint,
	const FRotator& RotationGrid)
{
	if (!bIncrementSnap && !bEditorSnap)
	{
		return AngleRadians;
	}
	const EBlendViewAxisConstraint Axis = FBlendViewTransformConstraintUtils::IsPlane(Constraint)
		? FBlendViewTransformConstraintUtils::GetPlaneExcludedAxis(Constraint)
		: Constraint;
	double GridDegrees = RotationGrid.Yaw;
	if (Axis == EBlendViewAxisConstraint::X) GridDegrees = RotationGrid.Roll;
	else if (Axis == EBlendViewAxisConstraint::Y) GridDegrees = RotationGrid.Pitch;
	GridDegrees = FMath::Max(FMath::Abs(GridDegrees), UE_SMALL_NUMBER);
	return FMath::DegreesToRadians(FBlendViewSnapSolver::SnapScalarToGrid(
		FMath::RadiansToDegrees(AngleRadians),
		GridDegrees));
}

double FBlendViewTransformSnapPolicy::SnapScaleFactor(
	const double ScaleFactor,
	const bool bIncrementSnap,
	const bool bPrecisionMode,
	const bool bNumericInput,
	const bool bEditorSnap,
	double EditorScaleStep,
	const bool bPercentageBasedScaling)
{
	if (bIncrementSnap && !bNumericInput)
	{
		return FBlendViewSnapSolver::SnapScalarToGrid(
			ScaleFactor,
			bPrecisionMode ? 0.01 : 0.1);
	}
	if (!bEditorSnap)
	{
		return ScaleFactor;
	}
	if (bPercentageBasedScaling || EditorScaleStep > 1.0)
	{
		EditorScaleStep /= 100.0;
	}
	EditorScaleStep = FMath::Max(EditorScaleStep, 0.01);
	return 1.0 + FBlendViewSnapSolver::SnapScalarToGrid(ScaleFactor - 1.0, EditorScaleStep);
}

FVector FBlendViewTransformSnapPolicy::BuildScaleFactorVector(
	const double ScaleFactor,
	const EBlendViewAxisConstraint Constraint)
{
	if (Constraint == EBlendViewAxisConstraint::None)
	{
		return FVector(ScaleFactor);
	}
	FVector Result = FVector::OneVector;
	if (Constraint == EBlendViewAxisConstraint::X) Result.X = ScaleFactor;
	else if (Constraint == EBlendViewAxisConstraint::Y) Result.Y = ScaleFactor;
	else if (Constraint == EBlendViewAxisConstraint::Z) Result.Z = ScaleFactor;
	else if (Constraint == EBlendViewAxisConstraint::PlaneX) Result.Y = Result.Z = ScaleFactor;
	else if (Constraint == EBlendViewAxisConstraint::PlaneY) Result.X = Result.Z = ScaleFactor;
	else if (Constraint == EBlendViewAxisConstraint::PlaneZ) Result.X = Result.Y = ScaleFactor;
	return Result;
}
