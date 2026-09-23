// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewTransformValueFormatter.h"

#include "Tools/BlendViewTransformPrecision.h"

namespace
{
	double CleanNearZero(const double Value, const int32 DecimalPlaces)
	{
		const double Threshold = 0.5 * FMath::Pow(10.0, -DecimalPlaces);
		return FMath::Abs(Value) < Threshold ? 0.0 : Value;
	}

	FString Fixed(const double Value, const int32 DecimalPlaces)
	{
		const double CleanValue = FBlendViewTransformPrecision::CleanNearInteger(Value);
		return FString::Printf(
			TEXT("%.*f"),
			DecimalPlaces,
			CleanNearZero(CleanValue, DecimalPlaces));
	}

	int32 GetMeterDecimalPlaces(const double Value)
	{
		const double Magnitude = FMath::Abs(FBlendViewTransformPrecision::CleanNearInteger(Value));
		if (Magnitude < 9.995)
		{
			return 3;
		}
		if (Magnitude < 99.95)
		{
			return 2;
		}
		if (Magnitude < 999.5)
		{
			return 1;
		}
		return 0;
	}

	FString WorldNumber(
		const double Value,
		const EBlendViewTranslationNumericUnit Unit,
		const int32 CentimeterDecimalPlaces)
	{
		return Unit == EBlendViewTranslationNumericUnit::Meters
			? Fixed(Value, GetMeterDecimalPlaces(Value))
			: Fixed(Value, CentimeterDecimalPlaces);
	}

	FString GraphNumber(const double Value)
	{
		const double CleanValue = CleanNearZero(Value, 3);
		if (FMath::IsNearlyEqual(CleanValue, FMath::RoundToDouble(CleanValue), 0.0005))
		{
			return FString::Printf(TEXT("%.0f"), CleanValue);
		}
		return Fixed(CleanValue, 3);
	}

	int32 GetAxisIndex(const EBlendViewAxisConstraint Constraint)
	{
		switch (Constraint)
		{
		case EBlendViewAxisConstraint::X:
			return 0;
		case EBlendViewAxisConstraint::Y:
			return 1;
		case EBlendViewAxisConstraint::Z:
			return 2;
		default:
			return INDEX_NONE;
		}
	}

	enum class EConstraintPhrase : uint8
	{
		Translation,
		Rotation
	};

	FString GetConstraintSuffix(
		const EBlendViewAxisConstraint Constraint,
		const bool bLocal,
		const bool bChinese,
		const EConstraintPhrase Phrase,
		const bool bGraph)
	{
		const TCHAR* Space = bLocal
			? (bChinese ? TEXT("局部") : TEXT("local"))
			: (bChinese ? TEXT("全局") : TEXT("global"));

		const TCHAR* AxisOrPlane = nullptr;
		switch (Constraint)
		{
		case EBlendViewAxisConstraint::X:
			AxisOrPlane = TEXT("X");
			break;
		case EBlendViewAxisConstraint::Y:
			AxisOrPlane = TEXT("Y");
			break;
		case EBlendViewAxisConstraint::Z:
			AxisOrPlane = TEXT("Z");
			break;
		case EBlendViewAxisConstraint::PlaneX:
			AxisOrPlane = TEXT("YZ");
			break;
		case EBlendViewAxisConstraint::PlaneY:
			AxisOrPlane = TEXT("XZ");
			break;
		case EBlendViewAxisConstraint::PlaneZ:
			AxisOrPlane = TEXT("XY");
			break;
		default:
			return FString();
		}

		const bool bPlaneConstraint = FCString::Strlen(AxisOrPlane) > 1;
		const TCHAR* ChinesePreposition =
			Phrase == EConstraintPhrase::Rotation ? TEXT("绕") : TEXT("沿");
		const TCHAR* EnglishPreposition =
			Phrase == EConstraintPhrase::Rotation ? TEXT("around") : TEXT("along");
		if (bGraph)
		{
			if (bChinese)
			{
				return FString::Printf(
					TEXT(" %s %s%s"),
					ChinesePreposition,
					AxisOrPlane,
					bPlaneConstraint ? TEXT(" 平面") : TEXT(" 轴"));
			}
			return FString::Printf(
				TEXT(" %s %s%s"),
				EnglishPreposition,
				AxisOrPlane,
				bPlaneConstraint ? TEXT(" plane") : TEXT(""));
		}

		if (bChinese)
		{
			return FString::Printf(
				TEXT(" %s %s %s%s"),
				ChinesePreposition,
				Space,
				AxisOrPlane,
				bPlaneConstraint ? TEXT(" 平面") : TEXT(" 轴"));
		}

		return FString::Printf(
			TEXT(" %s %s %s%s"),
			EnglishPreposition,
			Space,
			AxisOrPlane,
			bPlaneConstraint ? TEXT(" plane") : TEXT(""));
	}

	FString FormatTranslation(
		const FBlendViewTransformValueState& State,
		const EBlendViewTranslationNumericUnit Unit,
		const bool bChinese)
	{
		const FString Prefix = State.bPivotEditMode
			? (bChinese ? FString(TEXT("\u67A2\u8F74\u70B9 ")) : FString(TEXT("Pivot ")))
			: FString();

		if (State.bGraph)
		{
			if (const int32 AxisIndex = GetAxisIndex(State.Constraint); AxisIndex != INDEX_NONE)
			{
				return FString::Printf(
					TEXT("%sD: %s%s"),
					*Prefix,
					*GraphNumber(State.Translation[AxisIndex]),
					*GetConstraintSuffix(
						State.Constraint,
						false,
						bChinese,
						EConstraintPhrase::Translation,
						true));
			}
			return FString::Printf(
				TEXT("%sDx: %s  Dy: %s"),
				*Prefix,
				*GraphNumber(State.Translation.X),
				*GraphNumber(State.Translation.Y));
		}

		const double UnitScale = Unit == EBlendViewTranslationNumericUnit::Meters ? 100.0 : 1.0;
		const TCHAR* UnitText = Unit == EBlendViewTranslationNumericUnit::Meters ? TEXT("m") : TEXT("cm");
		const FVector DisplayTranslation = State.Translation / UnitScale;
		const double DisplayDistance = State.Translation.Size() / UnitScale;
		if (const int32 AxisIndex = GetAxisIndex(State.Constraint); AxisIndex != INDEX_NONE)
		{
			return FString::Printf(
				TEXT("%sD: %s %s (%s %s)%s"),
				*Prefix,
				*WorldNumber(DisplayTranslation[AxisIndex], Unit, 2),
				UnitText,
				*WorldNumber(DisplayDistance, Unit, 2),
				UnitText,
				*GetConstraintSuffix(
					State.Constraint,
					State.bLocalConstraint,
					bChinese,
					EConstraintPhrase::Translation,
					false));
		}

		return FString::Printf(
			TEXT("%sDx: %s %s   Dy: %s %s   Dz: %s %s (%s %s)%s"),
			*Prefix,
			*WorldNumber(DisplayTranslation.X, Unit, 3),
			UnitText,
			*WorldNumber(DisplayTranslation.Y, Unit, 3),
			UnitText,
			*WorldNumber(DisplayTranslation.Z, Unit, 3),
			UnitText,
			*WorldNumber(DisplayDistance, Unit, 2),
			UnitText,
			*GetConstraintSuffix(
				State.Constraint,
				State.bLocalConstraint,
				bChinese,
				EConstraintPhrase::Translation,
				false));
	}

	FString FormatRotation(const FBlendViewTransformValueState& State, const bool bChinese)
	{
		const FString Value = Fixed(State.RotationDegrees, 2);
		const FString Suffix =
			GetConstraintSuffix(
				State.Constraint,
				State.bLocalConstraint,
				bChinese,
				EConstraintPhrase::Rotation,
				State.bGraph);
		if (State.bTrackball)
		{
			return bChinese
				? FString::Printf(TEXT("轨迹球: %s%s"), *Value, *Suffix)
				: FString::Printf(TEXT("Trackball: %s%s"), *Value, *Suffix);
		}
		return bChinese
			? FString::Printf(TEXT("旋转: %s%s"), *Value, *Suffix)
			: FString::Printf(TEXT("Rotation: %s%s"), *Value, *Suffix);
	}

	FString FormatScale(const FBlendViewTransformValueState& State, const bool bChinese)
	{
		if (const int32 AxisIndex = GetAxisIndex(State.Constraint); AxisIndex != INDEX_NONE)
		{
			static const TCHAR* AxisNames[] = {TEXT("X"), TEXT("Y"), TEXT("Z")};
			if (bChinese)
			{
				return State.bGraph
					? FString::Printf(
						TEXT("%s 向缩放: %s"),
						AxisNames[AxisIndex],
						*Fixed(State.Scale[AxisIndex], 4))
					: FString::Printf(
						TEXT("%s %s 向缩放: %s"),
						State.bLocalConstraint ? TEXT("局部") : TEXT("全局"),
						AxisNames[AxisIndex],
						*Fixed(State.Scale[AxisIndex], 4));
			}
			return State.bGraph
				? FString::Printf(
					TEXT("Scale %s: %s"),
					AxisNames[AxisIndex],
					*Fixed(State.Scale[AxisIndex], 4))
				: FString::Printf(
					TEXT("Scale along %s %s: %s"),
					State.bLocalConstraint ? TEXT("local") : TEXT("global"),
					AxisNames[AxisIndex],
					*Fixed(State.Scale[AxisIndex], 4));
		}

		if (State.bGraph)
		{
			return bChinese
				? FString::Printf(
					TEXT("X 向缩放: %s  Y 向缩放: %s"),
					*Fixed(State.Scale.X, 4),
					*Fixed(State.Scale.Y, 4))
				: FString::Printf(
					TEXT("Scale X: %s  Y: %s"),
					*Fixed(State.Scale.X, 4),
					*Fixed(State.Scale.Y, 4));
		}
		return bChinese
			? FString::Printf(
				TEXT("X 向缩放: %s  Y 向缩放: %s  Z 向缩放: %s"),
				*Fixed(State.Scale.X, 4),
				*Fixed(State.Scale.Y, 4),
				*Fixed(State.Scale.Z, 4))
			: FString::Printf(
				TEXT("Scale X: %s  Y: %s  Z: %s"),
				*Fixed(State.Scale.X, 4),
				*Fixed(State.Scale.Y, 4),
				*Fixed(State.Scale.Z, 4));
	}

	const TCHAR* GetMirrorAxisName(const EBlendViewAxisConstraint Constraint)
	{
		switch (Constraint)
		{
		case EBlendViewAxisConstraint::X:
		case EBlendViewAxisConstraint::PlaneX:
			return TEXT("X");
		case EBlendViewAxisConstraint::Y:
		case EBlendViewAxisConstraint::PlaneY:
			return TEXT("Y");
		case EBlendViewAxisConstraint::Z:
		case EBlendViewAxisConstraint::PlaneZ:
			return TEXT("Z");
		default:
			return TEXT("");
		}
	}

	bool IsMirrorLockingConstraint(const EBlendViewAxisConstraint Constraint)
	{
		return Constraint == EBlendViewAxisConstraint::PlaneX ||
			Constraint == EBlendViewAxisConstraint::PlaneY ||
			Constraint == EBlendViewAxisConstraint::PlaneZ;
	}

	FString FormatMirror(const FBlendViewTransformValueState& State, const bool bChinese)
	{
		if (State.Constraint == EBlendViewAxisConstraint::None)
		{
			return bChinese
				? FString(TEXT("选择一个镜像轴 (X, Y, Z)"))
				: FString(TEXT("Select a mirror axis (X, Y, Z)"));
		}

		const TCHAR* Space = State.bLocalConstraint
			? (bChinese ? TEXT("局部") : TEXT("local"))
			: (bChinese ? TEXT("全局") : TEXT("global"));
		const TCHAR* Axis = GetMirrorAxisName(State.Constraint);
		if (IsMirrorLockingConstraint(State.Constraint))
		{
			return bChinese
				? FString::Printf(TEXT("镜像 锁定 %s %s 向"), Space, Axis)
				: FString::Printf(TEXT("Mirror locking %s %s"), Space, Axis);
		}

		return bChinese
			? FString::Printf(TEXT("镜像 沿 %s %s 向"), Space, Axis)
			: FString::Printf(TEXT("Mirror along %s %s"), Space, Axis);
	}
}

FText FBlendViewTransformValueFormatter::Format(
	const FBlendViewTransformValueState& State,
	const EBlendViewTranslationNumericUnit TranslationUnit,
	const bool bChinese)
{
	switch (State.Mode)
	{
	case EBlendViewTransformMode::Translate:
		return FText::FromString(FormatTranslation(State, TranslationUnit, bChinese));
	case EBlendViewTransformMode::Rotate:
		return FText::FromString(FormatRotation(State, bChinese));
	case EBlendViewTransformMode::Scale:
		return FText::FromString(FormatScale(State, bChinese));
	case EBlendViewTransformMode::Mirror:
		return FText::FromString(FormatMirror(State, bChinese));
	default:
		return FText::GetEmpty();
	}
}
