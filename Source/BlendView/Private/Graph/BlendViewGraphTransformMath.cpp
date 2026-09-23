// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Graph/BlendViewGraphTransformMath.h"

FVector2f FBlendViewGraphTransformMath::CalculatePivot(
	const TConstArrayView<FVector2f> Positions)
{
	if (Positions.IsEmpty())
	{
		return FVector2f::ZeroVector;
	}

	FVector2f Minimum = Positions[0];
	FVector2f Maximum = Positions[0];
	for (const FVector2f& Position : Positions.RightChop(1))
	{
		Minimum.X = FMath::Min(Minimum.X, Position.X);
		Minimum.Y = FMath::Min(Minimum.Y, Position.Y);
		Maximum.X = FMath::Max(Maximum.X, Position.X);
		Maximum.Y = FMath::Max(Maximum.Y, Position.Y);
	}
	return (Minimum + Maximum) * 0.5f;
}

FVector2f FBlendViewGraphTransformMath::ConstrainTranslation(
	const FVector2f& Translation,
	const EBlendViewGraphConstraint Constraint)
{
	switch (Constraint)
	{
	case EBlendViewGraphConstraint::X:
		return FVector2f(Translation.X, 0.0f);
	case EBlendViewGraphConstraint::Y:
		return FVector2f(0.0f, Translation.Y);
	default:
		return Translation;
	}
}

FVector2f FBlendViewGraphTransformMath::RotatePosition(
	const FVector2f& Position,
	const FVector2f& Pivot,
	const double AngleRadians)
{
	const FVector2f Relative = Position - Pivot;
	const double CosAngle = FMath::Cos(AngleRadians);
	const double SinAngle = FMath::Sin(AngleRadians);
	return Pivot + FVector2f(
		static_cast<float>(Relative.X * CosAngle - Relative.Y * SinAngle),
		static_cast<float>(Relative.X * SinAngle + Relative.Y * CosAngle));
}

FVector2f FBlendViewGraphTransformMath::ScalePosition(
	const FVector2f& Position,
	const FVector2f& Pivot,
	const double ScaleFactor,
	const EBlendViewGraphConstraint Constraint)
{
	FVector2f Relative = Position - Pivot;
	if (Constraint != EBlendViewGraphConstraint::Y)
	{
		Relative.X *= static_cast<float>(ScaleFactor);
	}
	if (Constraint != EBlendViewGraphConstraint::X)
	{
		Relative.Y *= static_cast<float>(ScaleFactor);
	}
	return Pivot + Relative;
}

FVector2f FBlendViewGraphTransformMath::GetNumericTranslationDirection(
	const FVector2f& FreeMoveIntent,
	const EBlendViewGraphConstraint Constraint)
{
	if (Constraint == EBlendViewGraphConstraint::X)
	{
		return FVector2f(1.0f, 0.0f);
	}
	if (Constraint == EBlendViewGraphConstraint::Y)
	{
		return FVector2f(0.0f, 1.0f);
	}
	return FreeMoveIntent.IsNearlyZero()
		? FVector2f(1.0f, 0.0f)
		: FreeMoveIntent.GetSafeNormal();
}
