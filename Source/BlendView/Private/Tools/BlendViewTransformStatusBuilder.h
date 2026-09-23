// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/BlendViewTransformTypes.h"

struct FBlendViewTransformStatusState
{
	EBlendViewTransformMode Mode = EBlendViewTransformMode::Translate;
	EBlendViewAxisConstraint Constraint = EBlendViewAxisConstraint::None;
	bool bSelectingSnapBase = false;
	bool bHasSnapBase = false;
	bool bFreeRotate = false;
	bool bPivotEditMode = false;
	bool bModelingPivotEditMode = false;
	bool bSupportsZAxis = true;
	bool bSupportsPlaneConstraint = true;
	bool bSupportsAutoConstraint = true;
	bool bSupportsAutoConstraintPlane = true;
	bool bSupportsTrackball = true;
	bool bSupportsSnapBase = true;
	bool bSupportsPivotEdit = true;
	bool bUseResizeLabel = false;
	TOptional<FString> NumericBuffer;
};

struct FBlendViewTransformStatusShortcuts
{
	FText Translate;
	FText Rotate;
	FText Scale;
	FText Mirror;
	FText PivotEditMode;
	FText ClearConstraint;
	FText SetSnapBase;
	FText ToggleHints;
};

class FBlendViewTransformStatusBuilder final
{
public:
	static FBlendViewStatusLine Build(const FBlendViewTransformStatusState& State);
	static FBlendViewStatusLine Build(
		const FBlendViewTransformStatusState& State,
		const FBlendViewTransformStatusShortcuts& Shortcuts);
};
