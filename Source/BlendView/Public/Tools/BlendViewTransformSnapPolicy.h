// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/BlendViewTransformTypes.h"

class FBlendViewTransformSnapPolicy final
{
public:
	static FVector SnapTranslationDelta(
		const FVector& Delta,
		double GridSize,
		EBlendViewAxisConstraint Constraint,
		const FQuat& ConstraintRotation);
	static double SnapRotationRadians(
		double AngleRadians,
		bool bIncrementSnap,
		bool bEditorSnap,
		EBlendViewAxisConstraint Constraint,
		const FRotator& RotationGrid);
	static double SnapScaleFactor(
		double ScaleFactor,
		bool bIncrementSnap,
		bool bPrecisionMode,
		bool bNumericInput,
		bool bEditorSnap,
		double EditorScaleStep,
		bool bPercentageBasedScaling);
	static FVector BuildScaleFactorVector(
		double ScaleFactor,
		EBlendViewAxisConstraint Constraint);
};
