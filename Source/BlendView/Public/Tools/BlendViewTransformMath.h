// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewTransformMath
{
public:
	static FTransform BuildRotatedTransform(
		const FTransform& BaselineTransform,
		const FVector& PivotLocation,
		const FQuat& DeltaRotation);

	static FTransform BuildScaledTransform(
		const FTransform& BaselineTransform,
		const FVector& PivotLocation,
		const FVector& ScaleFactor,
		const FQuat& ConstraintRotation,
		bool bLocalConstraint);

	static FTransform BuildMirroredTransform(
		const FTransform& BaselineTransform,
		const FVector& PivotLocation,
		const FVector& MirrorScaleFactor,
		const FQuat& ConstraintRotation,
		bool bLocalConstraint);
};
