// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewPlaneTranslationSession
{
public:
	FVector Begin(
		const FVector& PointerAnchorLocation,
		const FVector& BaselineDelta,
		const FQuat& ConstraintRotation,
		int32 ExcludedAxisIndex);
	void Reset();

	bool IsActive() const { return bActive; }
	bool Matches(const FQuat& ConstraintRotation, int32 ExcludedAxisIndex) const;
	FVector Solve(const FVector& CurrentPointerLocation) const;
	FVector ConstrainDelta(const FVector& CandidateDelta) const;
	static FVector ProjectDeltaToPlane(
		const FVector& Delta,
		const FQuat& ConstraintRotation,
		int32 ExcludedAxisIndex);

private:
	FVector PointerAnchorLocation = FVector::ZeroVector;
	FVector BaselineDelta = FVector::ZeroVector;
	FQuat ConstraintRotation = FQuat::Identity;
	int32 ExcludedAxisIndex = INDEX_NONE;
	bool bActive = false;
};
