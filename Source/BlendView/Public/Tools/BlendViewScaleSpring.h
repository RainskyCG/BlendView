// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewScaleSpring final
{
public:
	void Begin(const FVector2D& PivotPosition, const FVector2D& PointerPosition);
	void Reset();
	bool Evaluate(const FVector2D& PointerPosition, double& OutScaleFactor) const;

private:
	FVector2D PivotPosition = FVector2D::ZeroVector;
	FVector2D InitialVector = FVector2D::ZeroVector;
	double InitialDistance = 0.0;
	bool bActive = false;
};
