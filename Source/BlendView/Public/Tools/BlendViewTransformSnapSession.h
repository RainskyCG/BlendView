// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Snap/BlendViewSnapSolver.h"

class FBlendViewTransformSnapSession final
{
public:
	void ClearActiveTarget()
	{
		bHasActiveTarget = false;
		bHasActiveTargetNormal = false;
		ActiveTargetKind = EBlendViewSnapTargetKind::None;
	}

	void SetActiveTarget(
		const FVector& Location,
		const FVector& Normal,
		const bool bHasNormal,
		const EBlendViewSnapTargetKind Kind)
	{
		bHasActiveTarget = true;
		ActiveTargetLocation = Location;
		ActiveTargetNormal = Normal.GetSafeNormal();
		bHasActiveTargetNormal = bHasNormal && !ActiveTargetNormal.IsNearlyZero();
		ActiveTargetKind = Kind;
	}

	void BeginBaseSelection()
	{
		bSelectingBase = true;
		bHasBaseCandidate = false;
	}

	void SetBaseCandidate(const FVector& Location, const EBlendViewSnapTargetKind Kind)
	{
		BaseCandidateLocation = Location;
		BaseCandidateKind = Kind;
		bHasBaseCandidate = true;
	}

	void ConfirmBaseSelection()
	{
		if (bHasBaseCandidate)
		{
			BaseLocation = BaseCandidateLocation;
			bHasBase = true;
		}
		bSelectingBase = false;
	}

	void CancelBaseSelection()
	{
		bSelectingBase = false;
		bHasBaseCandidate = false;
		BaseCandidateKind = EBlendViewSnapTargetKind::None;
	}

	void ResetTransient()
	{
		bSelectingBase = false;
		bHasBaseCandidate = false;
		BaseCandidateKind = EBlendViewSnapTargetKind::None;
		ClearActiveTarget();
	}

	bool bActiveForCurrentTranslation = false;
	bool bHasActiveTarget = false;
	bool bHasActiveTargetNormal = false;
	bool bSelectingBase = false;
	bool bHasBase = false;
	bool bHasBaseCandidate = false;
	FVector BaseLocation = FVector::ZeroVector;
	FVector BaseCandidateLocation = FVector::ZeroVector;
	FVector ActiveTargetLocation = FVector::ZeroVector;
	FVector ActiveTargetNormal = FVector::UpVector;
	EBlendViewSnapTargetKind BaseCandidateKind = EBlendViewSnapTargetKind::None;
	EBlendViewSnapTargetKind ActiveTargetKind = EBlendViewSnapTargetKind::None;
};
