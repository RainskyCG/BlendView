// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "BlendViewSettings.h"
#include "CoreMinimal.h"
#include "Tools/BlendViewTransformTypes.h"

struct FBlendViewTransformValueState
{
	EBlendViewTransformMode Mode = EBlendViewTransformMode::Translate;
	EBlendViewAxisConstraint Constraint = EBlendViewAxisConstraint::None;
	FVector Translation = FVector::ZeroVector;
	double RotationDegrees = 0.0;
	FVector Scale = FVector::OneVector;
	bool bLocalConstraint = false;
	bool bGraph = false;
	bool bTrackball = false;
	bool bPivotEditMode = false;
};

class FBlendViewTransformValueFormatter final
{
public:
	static FText Format(
		const FBlendViewTransformValueState& State,
		EBlendViewTranslationNumericUnit TranslationUnit,
		bool bChinese);
};
