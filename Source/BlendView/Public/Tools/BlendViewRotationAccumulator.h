// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewRotationAccumulator final
{
public:
	void Reset();
	bool Begin(const FVector& InitialDirection);
	bool Evaluate(
		const FVector& CurrentDirection,
		const FVector& SignedViewNormal,
		double& OutAngleRadians);

private:
	FVector PreviousDirection = FVector::ZeroVector;
	double AccumulatedAngleRadians = 0.0;
	bool bActive = false;
};
