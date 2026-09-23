// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EBlendViewTransformMode : uint8
{
	Translate,
	Rotate,
	Scale,
	Mirror
};

enum class EBlendViewTransformResetChannel : uint8
{
	Location,
	Rotation,
	Scale
};

enum class EBlendViewAxisConstraint : uint8
{
	None,
	X,
	Y,
	Z,
	PlaneX,
	PlaneY,
	PlaneZ
};

enum class EBlendViewStatusTokenKind : uint8
{
	Key,
	Mouse
};

struct FBlendViewStatusToken
{
	EBlendViewStatusTokenKind Kind = EBlendViewStatusTokenKind::Key;
	FText Text;
};

struct FBlendViewStatusHint
{
	TArray<FBlendViewStatusToken> Tokens;
	FText Label;
};

struct FBlendViewStatusLine
{
	TArray<FBlendViewStatusHint> Hints;
};
