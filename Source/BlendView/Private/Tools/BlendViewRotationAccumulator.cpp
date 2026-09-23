// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewRotationAccumulator.h"

void FBlendViewRotationAccumulator::Reset()
{
	PreviousDirection = FVector::ZeroVector;
	AccumulatedAngleRadians = 0.0;
	bActive = false;
}

bool FBlendViewRotationAccumulator::Begin(const FVector& InitialDirection)
{
	Reset();
	PreviousDirection = InitialDirection.GetSafeNormal();
	bActive = !PreviousDirection.IsNearlyZero();
	return bActive;
}

bool FBlendViewRotationAccumulator::Evaluate(
	const FVector& CurrentDirection,
	const FVector& SignedViewNormal,
	double& OutAngleRadians)
{
	const FVector NormalizedCurrentDirection = CurrentDirection.GetSafeNormal();
	const FVector NormalizedViewNormal = SignedViewNormal.GetSafeNormal();
	if (!bActive || NormalizedCurrentDirection.IsNearlyZero() || NormalizedViewNormal.IsNearlyZero())
	{
		return false;
	}

	const FVector Tangent = FVector::CrossProduct(
		PreviousDirection,
		NormalizedViewNormal).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		return false;
	}

	const double X = FMath::Clamp(
		static_cast<double>(FVector::DotProduct(NormalizedCurrentDirection, PreviousDirection)),
		-1.0,
		1.0);
	const double Y = static_cast<double>(FVector::DotProduct(NormalizedCurrentDirection, Tangent));
	AccumulatedAngleRadians += FMath::Atan2(Y, X);
	PreviousDirection = NormalizedCurrentDirection;
	OutAngleRadians = AccumulatedAngleRadians;
	return true;
}
