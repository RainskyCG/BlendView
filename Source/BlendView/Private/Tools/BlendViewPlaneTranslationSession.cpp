// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewPlaneTranslationSession.h"

FVector FBlendViewPlaneTranslationSession::Begin(
	const FVector& InPointerAnchorLocation,
	const FVector& InBaselineDelta,
	const FQuat& InConstraintRotation,
	const int32 InExcludedAxisIndex)
{
	PointerAnchorLocation = InPointerAnchorLocation;
	ConstraintRotation = InConstraintRotation.GetNormalized();
	ExcludedAxisIndex = InExcludedAxisIndex;
	bActive = ExcludedAxisIndex >= 0 && ExcludedAxisIndex < 3;
	BaselineDelta = bActive
		? ProjectDeltaToPlane(InBaselineDelta, ConstraintRotation, ExcludedAxisIndex)
		: InBaselineDelta;
	return BaselineDelta;
}

void FBlendViewPlaneTranslationSession::Reset()
{
	PointerAnchorLocation = FVector::ZeroVector;
	BaselineDelta = FVector::ZeroVector;
	ConstraintRotation = FQuat::Identity;
	ExcludedAxisIndex = INDEX_NONE;
	bActive = false;
}

bool FBlendViewPlaneTranslationSession::Matches(
	const FQuat& InConstraintRotation,
	const int32 InExcludedAxisIndex) const
{
	return bActive &&
		ExcludedAxisIndex == InExcludedAxisIndex &&
		ConstraintRotation.Equals(InConstraintRotation.GetNormalized());
}

FVector FBlendViewPlaneTranslationSession::Solve(const FVector& CurrentPointerLocation) const
{
	if (!bActive)
	{
		return BaselineDelta;
	}

	FVector PointerDeltaInConstraintSpace = ConstraintRotation.UnrotateVector(
		CurrentPointerLocation - PointerAnchorLocation);
	PointerDeltaInConstraintSpace[ExcludedAxisIndex] = 0.0;
	return BaselineDelta + ConstraintRotation.RotateVector(PointerDeltaInConstraintSpace);
}

FVector FBlendViewPlaneTranslationSession::ConstrainDelta(
	const FVector& CandidateDelta) const
{
	if (!bActive)
	{
		return CandidateDelta;
	}

	FVector CandidateInConstraintSpace = ConstraintRotation.UnrotateVector(CandidateDelta);
	const FVector BaselineInConstraintSpace = ConstraintRotation.UnrotateVector(BaselineDelta);
	CandidateInConstraintSpace[ExcludedAxisIndex] = BaselineInConstraintSpace[ExcludedAxisIndex];
	return ConstraintRotation.RotateVector(CandidateInConstraintSpace);
}

FVector FBlendViewPlaneTranslationSession::ProjectDeltaToPlane(
	const FVector& Delta,
	const FQuat& InConstraintRotation,
	const int32 InExcludedAxisIndex)
{
	if (InExcludedAxisIndex < 0 || InExcludedAxisIndex >= 3)
	{
		return Delta;
	}

	const FQuat NormalizedRotation = InConstraintRotation.GetNormalized();
	FVector DeltaInConstraintSpace = NormalizedRotation.UnrotateVector(Delta);
	DeltaInConstraintSpace[InExcludedAxisIndex] = 0.0;
	return NormalizedRotation.RotateVector(DeltaInConstraintSpace);
}
