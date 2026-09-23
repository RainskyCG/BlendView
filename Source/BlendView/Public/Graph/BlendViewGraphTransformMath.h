// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EBlendViewGraphConstraint : uint8
{
	None,
	X,
	Y
};

class FBlendViewGraphTransformMath final
{
public:
	static FVector2f CalculatePivot(TConstArrayView<FVector2f> Positions);
	static FVector2f ConstrainTranslation(
		const FVector2f& Translation,
		EBlendViewGraphConstraint Constraint);
	static FVector2f RotatePosition(
		const FVector2f& Position,
		const FVector2f& Pivot,
		double AngleRadians);
	static FVector2f ScalePosition(
		const FVector2f& Position,
		const FVector2f& Pivot,
		double ScaleFactor,
		EBlendViewGraphConstraint Constraint);
	static FVector2f GetNumericTranslationDirection(
		const FVector2f& FreeMoveIntent,
		EBlendViewGraphConstraint Constraint);
};
