// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/BlendViewTransformTypes.h"

class FBlendViewTransformConstraintUtils final
{
public:
	static EBlendViewAxisConstraint AxisToPlane(EBlendViewAxisConstraint Axis);
	static EBlendViewAxisConstraint GetPlaneExcludedAxis(EBlendViewAxisConstraint Plane);
	static bool IsSingleAxis(EBlendViewAxisConstraint Constraint);
	static bool IsPlane(EBlendViewAxisConstraint Constraint);
	static bool IsAxisActive(EBlendViewAxisConstraint Constraint, EBlendViewAxisConstraint Axis);
	static FQuat GetSpaceRotation(bool bLocalConstraint, const FQuat& LocalRotation);
	static FVector RemovePlaneExcludedComponent(FVector LocalVector, EBlendViewAxisConstraint Plane);
	static FVector ProjectVectorOntoPlane(
		const FVector& Vector,
		EBlendViewAxisConstraint Plane,
		const FQuat& ConstraintRotation);
	static FVector GetConstraintAxisVector(
		EBlendViewAxisConstraint Constraint,
		double AxisSign,
		bool bLocalConstraint,
		const FQuat& LocalRotation);
	static FVector ApplyVectorConstraint(
		const FVector& Vector,
		EBlendViewAxisConstraint Constraint,
		double AxisSign,
		bool bLocalConstraint,
		const FQuat& LocalRotation);
	static FVector GetAxisVector(
		EBlendViewAxisConstraint Axis,
		bool bLocalConstraint,
		const FQuat& LocalRotation);
};
