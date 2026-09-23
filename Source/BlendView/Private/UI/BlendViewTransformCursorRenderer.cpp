// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewTransformCursorRenderer.h"

#include "Brushes/SlateImageBrush.h"
#include "Misc/AxisDisplayInfo.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"

namespace
{
	constexpr float BlendViewCursorExtent = 20.0f;
	constexpr float BlendViewCursorHalfSeparation = 14.0f;

	const FSlateBrush* GetBlendViewRotateScaleCursorBrush()
	{
		static const FSlateVectorImageBrush Brush(
			FPaths::EngineContentDir() / TEXT("Slate/Starship/Common/chevron-up.svg"),
			FVector2D(BlendViewCursorExtent, BlendViewCursorExtent));
		return &Brush;
	}

	int32 PaintTrackballCursor(
		const FGeometry& Geometry,
		FSlateWindowElementList& DrawElements,
		const int32 LayerId,
		const FVector2D& Cursor,
		const FSlateBrush* CursorBrush)
	{
		const FVector2D CursorSize(CursorBrush->ImageSize);
		const FLinearColor XColor = AxisDisplayInfo::GetAxisColor(EAxisList::X);
		const FLinearColor YColor = AxisDisplayInfo::GetAxisColor(EAxisList::Y);
		const int32 CursorLayer = LayerId + 1;
		struct FArrow
		{
			FVector2D Offset;
			float Angle;
			FLinearColor Color;
		};
		const FArrow Arrows[] = {
			{FVector2D(0.0, -BlendViewCursorHalfSeparation), 0.0f, YColor},
			{FVector2D(0.0, BlendViewCursorHalfSeparation), UE_PI, YColor},
			{FVector2D(BlendViewCursorHalfSeparation, 0.0), UE_HALF_PI, XColor},
			{FVector2D(-BlendViewCursorHalfSeparation, 0.0), -UE_HALF_PI, XColor}
		};
		for (const FArrow& Arrow : Arrows)
		{
			const FVector2D ArrowCenter = Cursor + Arrow.Offset;
			FSlateDrawElement::MakeRotatedBox(
				DrawElements,
				CursorLayer,
				Geometry.ToPaintGeometry(
					CursorSize,
					FSlateLayoutTransform(ArrowCenter - CursorSize * 0.5)),
				CursorBrush,
				ESlateDrawEffect::None,
				Arrow.Angle,
				TOptional<FVector2f>(),
				FSlateDrawElement::RelativeToElement,
				Arrow.Color);
		}
		return CursorLayer;
	}
}

int32 BlendViewTransformCursorRenderer::PaintSoftwareCursor(
	const FGeometry& Geometry,
	FSlateWindowElementList& DrawElements,
	const int32 LayerId,
	const EBlendViewTransformMode Mode,
	const FVector2D& Pivot,
	const FVector2D& Cursor,
	const bool bTrackball)
{
	if (Mode == EBlendViewTransformMode::Translate ||
		Mode == EBlendViewTransformMode::Mirror)
	{
		return LayerId;
	}

	const FSlateBrush* CursorBrush = GetBlendViewRotateScaleCursorBrush();
	if (!CursorBrush)
	{
		return LayerId;
	}
	if (bTrackball)
	{
		return PaintTrackballCursor(Geometry, DrawElements, LayerId, Cursor, CursorBrush);
	}

	const FVector2D RadialDirection = Cursor - Pivot;
	const float RadialAngle = RadialDirection.IsNearlyZero()
		? 0.0f
		: FMath::Atan2(
			static_cast<float>(RadialDirection.Y),
			static_cast<float>(RadialDirection.X));
	const float CursorAngle = Mode == EBlendViewTransformMode::Scale
		? RadialAngle - UE_HALF_PI
		: RadialAngle;
	const float CosAngle = FMath::Cos(CursorAngle);
	const float SinAngle = FMath::Sin(CursorAngle);
	auto RotateOffset = [CosAngle, SinAngle](const FVector2D& Offset)
	{
		return FVector2D(
			Offset.X * CosAngle - Offset.Y * SinAngle,
			Offset.X * SinAngle + Offset.Y * CosAngle);
	};

	const FVector2D CursorSize(CursorBrush->ImageSize);
	const int32 CursorLayer = LayerId + 1;
	for (const double Direction : {-1.0, 1.0})
	{
		const FVector2D ArrowCenter = Cursor + RotateOffset(
			FVector2D(0.0, Direction * BlendViewCursorHalfSeparation));
		const FPaintGeometry ArrowGeometry = Geometry.ToPaintGeometry(
			CursorSize,
			FSlateLayoutTransform(ArrowCenter - CursorSize * 0.5));
		FSlateDrawElement::MakeRotatedBox(
			DrawElements,
			CursorLayer,
			ArrowGeometry,
			CursorBrush,
			ESlateDrawEffect::None,
			CursorAngle + (Direction > 0.0 ? UE_PI : 0.0),
			TOptional<FVector2f>(),
			FSlateDrawElement::RelativeToElement,
			FLinearColor::White);
	}
	return CursorLayer;
}
