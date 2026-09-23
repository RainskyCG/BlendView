// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewScaleSpring.h"

void FBlendViewScaleSpring::Begin(
	const FVector2D& InPivotPosition,
	const FVector2D& PointerPosition)
{
	PivotPosition = InPivotPosition;
	InitialVector = PointerPosition - PivotPosition;
	InitialDistance = FMath::Max(InitialVector.Size(), 1.0);
	bActive = true;
}

void FBlendViewScaleSpring::Reset()
{
	PivotPosition = FVector2D::ZeroVector;
	InitialVector = FVector2D::ZeroVector;
	InitialDistance = 0.0;
	bActive = false;
}

bool FBlendViewScaleSpring::Evaluate(
	const FVector2D& PointerPosition,
	double& OutScaleFactor) const
{
	if (!bActive)
	{
		return false;
	}

	const FVector2D CurrentVector = PointerPosition - PivotPosition;
	const double SignDot = FVector2D::DotProduct(CurrentVector, InitialVector);
	const double ScaleMagnitude = CurrentVector.Size() / InitialDistance;
	OutScaleFactor = SignDot < 0.0 ? -ScaleMagnitude : ScaleMagnitude;
	return true;
}
