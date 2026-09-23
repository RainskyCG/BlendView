// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewTransformConstraintUtils.h"

EBlendViewAxisConstraint FBlendViewTransformConstraintUtils::AxisToPlane(
	const EBlendViewAxisConstraint Axis)
{
	switch (Axis)
	{
	case EBlendViewAxisConstraint::X: return EBlendViewAxisConstraint::PlaneX;
	case EBlendViewAxisConstraint::Y: return EBlendViewAxisConstraint::PlaneY;
	case EBlendViewAxisConstraint::Z: return EBlendViewAxisConstraint::PlaneZ;
	default: return EBlendViewAxisConstraint::None;
	}
}

EBlendViewAxisConstraint FBlendViewTransformConstraintUtils::GetPlaneExcludedAxis(
	const EBlendViewAxisConstraint Plane)
{
	switch (Plane)
	{
	case EBlendViewAxisConstraint::PlaneX: return EBlendViewAxisConstraint::X;
	case EBlendViewAxisConstraint::PlaneY: return EBlendViewAxisConstraint::Y;
	case EBlendViewAxisConstraint::PlaneZ: return EBlendViewAxisConstraint::Z;
	default: return EBlendViewAxisConstraint::None;
	}
}

bool FBlendViewTransformConstraintUtils::IsSingleAxis(const EBlendViewAxisConstraint Constraint)
{
	return Constraint == EBlendViewAxisConstraint::X ||
		Constraint == EBlendViewAxisConstraint::Y ||
		Constraint == EBlendViewAxisConstraint::Z;
}

bool FBlendViewTransformConstraintUtils::IsPlane(const EBlendViewAxisConstraint Constraint)
{
	return Constraint == EBlendViewAxisConstraint::PlaneX ||
		Constraint == EBlendViewAxisConstraint::PlaneY ||
		Constraint == EBlendViewAxisConstraint::PlaneZ;
}

bool FBlendViewTransformConstraintUtils::IsAxisActive(
	const EBlendViewAxisConstraint Constraint,
	const EBlendViewAxisConstraint Axis)
{
	if (Constraint == Axis)
	{
		return true;
	}
	switch (Constraint)
	{
	case EBlendViewAxisConstraint::PlaneX:
		return Axis == EBlendViewAxisConstraint::Y || Axis == EBlendViewAxisConstraint::Z;
	case EBlendViewAxisConstraint::PlaneY:
		return Axis == EBlendViewAxisConstraint::X || Axis == EBlendViewAxisConstraint::Z;
	case EBlendViewAxisConstraint::PlaneZ:
		return Axis == EBlendViewAxisConstraint::X || Axis == EBlendViewAxisConstraint::Y;
	default:
		return false;
	}
}

FQuat FBlendViewTransformConstraintUtils::GetSpaceRotation(
	const bool bLocalConstraint,
	const FQuat& LocalRotation)
{
	return bLocalConstraint ? LocalRotation : FQuat::Identity;
}

FVector FBlendViewTransformConstraintUtils::RemovePlaneExcludedComponent(
	FVector LocalVector,
	const EBlendViewAxisConstraint Plane)
{
	switch (GetPlaneExcludedAxis(Plane))
	{
	case EBlendViewAxisConstraint::X:
		LocalVector.X = 0.0;
		break;
	case EBlendViewAxisConstraint::Y:
		LocalVector.Y = 0.0;
		break;
	case EBlendViewAxisConstraint::Z:
		LocalVector.Z = 0.0;
		break;
	default:
		break;
	}
	return LocalVector;
}

FVector FBlendViewTransformConstraintUtils::ProjectVectorOntoPlane(
	const FVector& Vector,
	const EBlendViewAxisConstraint Plane,
	const FQuat& ConstraintRotation)
{
	return ConstraintRotation.RotateVector(
		RemovePlaneExcludedComponent(ConstraintRotation.UnrotateVector(Vector), Plane));
}

FVector FBlendViewTransformConstraintUtils::GetConstraintAxisVector(
	const EBlendViewAxisConstraint Constraint,
	const double AxisSign,
	const bool bLocalConstraint,
	const FQuat& LocalRotation)
{
	return GetAxisVector(Constraint, bLocalConstraint, LocalRotation) * AxisSign;
}

FVector FBlendViewTransformConstraintUtils::ApplyVectorConstraint(
	const FVector& Vector,
	const EBlendViewAxisConstraint Constraint,
	const double AxisSign,
	const bool bLocalConstraint,
	const FQuat& LocalRotation)
{
	if (Constraint == EBlendViewAxisConstraint::None)
	{
		return Vector;
	}

	if (IsPlane(Constraint))
	{
		return ProjectVectorOntoPlane(
			Vector,
			Constraint,
			GetSpaceRotation(bLocalConstraint, LocalRotation));
	}

	const FVector Axis = GetConstraintAxisVector(
		Constraint,
		AxisSign,
		bLocalConstraint,
		LocalRotation);
	return Axis * FVector::DotProduct(Vector, Axis);
}

FVector FBlendViewTransformConstraintUtils::GetAxisVector(
	const EBlendViewAxisConstraint Axis,
	const bool bLocalConstraint,
	const FQuat& LocalRotation)
{
	FVector BaseAxis = FVector::ZeroVector;
	switch (Axis)
	{
	case EBlendViewAxisConstraint::X: BaseAxis = FVector::XAxisVector; break;
	case EBlendViewAxisConstraint::Y: BaseAxis = FVector::YAxisVector; break;
	case EBlendViewAxisConstraint::Z: BaseAxis = FVector::ZAxisVector; break;
	default: return FVector::ZeroVector;
	}
	return GetSpaceRotation(bLocalConstraint, LocalRotation).RotateVector(BaseAxis).GetSafeNormal();
}
