// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/DrawElements.h"

namespace BlendViewGuideDrawing
{
	inline int32 DrawSlateDashedGuide(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& Geometry,
		const int32 LayerId,
		const FVector2D& Start,
		const FVector2D& End)
	{
		constexpr float DashLengthPixels = 6.0f;
		constexpr float GapLengthPixels = 5.0f;
		constexpr float ShadowOffsetPixels = 1.0f;
		constexpr float ShadowThicknessPixels = 1.0f;
		constexpr float LineThicknessPixels = 1.0f;

		const FVector2D Delta = End - Start;
		const float Length = Delta.Size();
		if (Length <= UE_SMALL_NUMBER)
		{
			return LayerId;
		}

		const FVector2D Direction = Delta / Length;
		for (float Offset = 0.0f; Offset < Length; Offset += DashLengthPixels + GapLengthPixels)
		{
			const float SegmentEndOffset = FMath::Min(Offset + DashLengthPixels, Length);
			const FVector2D SegmentStart = Start + Direction * Offset;
			const FVector2D SegmentEnd = Start + Direction * SegmentEndOffset;

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				Geometry.ToPaintGeometry(),
				{SegmentStart + FVector2D(0.0f, ShadowOffsetPixels), SegmentEnd + FVector2D(0.0f, ShadowOffsetPixels)},
				ESlateDrawEffect::None,
				FLinearColor(0.0f, 0.0f, 0.0f, 0.55f),
				true,
				ShadowThicknessPixels);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				Geometry.ToPaintGeometry(),
				{SegmentStart, SegmentEnd},
				ESlateDrawEffect::None,
				FLinearColor::White,
				true,
				LineThicknessPixels);
		}

		return LayerId + 1;
	}
}
