// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewTransformPrecision final
{
public:
	static double CleanNearInteger(const double Value)
	{
		constexpr double FourDecimalHalfStep = 0.0000501;
		const double Rounded = FMath::RoundToDouble(Value);
		return FMath::Abs(Value - Rounded) <= FourDecimalHalfStep ? Rounded : Value;
	}

	static FVector CleanNearInteger(const FVector& Value)
	{
		return FVector(
			CleanNearInteger(Value.X),
			CleanNearInteger(Value.Y),
			CleanNearInteger(Value.Z));
	}

	static FVector2f CleanNearInteger(const FVector2f& Value)
	{
		return FVector2f(
			static_cast<float>(CleanNearInteger(Value.X)),
			static_cast<float>(CleanNearInteger(Value.Y)));
	}

	static FRotator CleanNearInteger(const FRotator& Value)
	{
		return FRotator(
			CleanNearInteger(Value.Pitch),
			CleanNearInteger(Value.Yaw),
			CleanNearInteger(Value.Roll));
	}

	static FQuat CleanNearIntegerRotation(const FQuat& Value)
	{
		return CleanNearInteger(Value.Rotator()).Quaternion().GetNormalized();
	}
};
