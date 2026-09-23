// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewTransformOverlay.h"

#include "BlendViewSettings.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/BlendViewGuideDrawing.h"
#include "UI/BlendViewTransformCursorRenderer.h"

namespace
{
	constexpr float DefaultAxisLineThicknessAt100Percent = 3.0f;
	constexpr float PivotMarkerOuterRadiusPixels = 4.5f;
	constexpr float PivotMarkerInnerRadiusPixels = 3.25f;
	constexpr float SnapBaseMarkerHalfSizePixels = 7.0f;
	constexpr float SnapBaseMarkerThicknessPixels = 2.5f;
	constexpr float TransformValueTopInsetPixels = 8.0f;
	constexpr float TransformValueHorizontalPaddingPixels = 12.0f;
	constexpr float TransformValueVerticalPaddingPixels = 4.0f;
	const FLinearColor TransformValueBackgroundColor(0.035f, 0.035f, 0.035f, 0.78f);
	const FLinearColor SceneTransformValueTextColor =
		FLinearColor::FromSRGBColor(FColor(0xE6, 0xE6, 0xE6, 0xFF));

	const FSlateBrush* GetTransformValueBackgroundBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 3.0f);
		return &Brush;
	}
}

float BlendViewTransformOverlay::GetDPIScale()
{
	return FSlateApplication::IsInitialized()
		? FMath::Max(
			FSlateApplication::Get().GetApplicationScale(),
			static_cast<float>(UE_SMALL_NUMBER))
		: 1.0f;
}

float BlendViewTransformOverlay::GetAxisLineThicknessPixels()
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	const float BaseThickness = Settings
		? Settings->SnapAxisThickness
		: DefaultAxisLineThicknessAt100Percent;
	return FMath::Max(BaseThickness, 0.5f) * GetDPIScale();
}

void SBlendViewTransformOverlay::SetDrawingData(
	const FVector2D& InAxisPivot,
	const FVector2D& InPivotPoint,
	const TArray<FBlendViewTransformOverlayAxis>& InAxes,
	const TOptional<FVector2D>& InGuideStart,
	const TOptional<FVector2D>& InGuideEnd,
	const TOptional<FVector2D>& InSnapBaseMarker,
	const EBlendViewSnapTargetKind InSnapBaseMarkerKind,
	const FLinearColor& InSnapBaseMarkerColor,
	const bool bInDrawPivotMarker)
{
	AxisPivot = InAxisPivot;
	PivotPoint = InPivotPoint;
	Axes = InAxes;
	GuideStart = InGuideStart;
	GuideEnd = InGuideEnd;
	SnapBaseMarker = InSnapBaseMarker;
	SnapBaseMarkerKind = InSnapBaseMarkerKind;
	SnapBaseMarkerColor = InSnapBaseMarkerColor;
	bDrawPivotMarker = bInDrawPivotMarker;
	bHasDrawingData = true;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlendViewTransformOverlay::SetCursorData(
	const EBlendViewTransformMode InMode,
	const FVector2D& InPivotPoint,
	const FVector2D& InCursorPoint,
	const bool bInTrackball)
{
	CursorMode = InMode;
	CursorPivotPoint = InPivotPoint;
	CursorPoint = InCursorPoint;
	bTrackballCursor = bInTrackball;
	bHasCursorData = true;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlendViewTransformOverlay::ClearCursorData()
{
	bHasCursorData = false;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlendViewTransformOverlay::ClearDrawingData()
{
	Axes.Reset();
	GuideStart.Reset();
	GuideEnd.Reset();
	SnapBaseMarker.Reset();
	SnapBaseMarkerKind = EBlendViewSnapTargetKind::None;
	bHasDrawingData = false;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlendViewTransformOverlay::SetValueText(const FText& InValueText)
{
	ValueText = InValueText;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SBlendViewTransformOverlay::ComputeDesiredSize(float) const
{
	return FVector2D::ZeroVector;
}

int32 SBlendViewTransformOverlay::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const int32 ValueLayer = PaintTransformValue(
		AllottedGeometry,
		OutDrawElements,
		LayerId + 100,
		Size);
	if (!bHasDrawingData || Size.X <= 1.0f || Size.Y <= 1.0f)
	{
		return FMath::Max(
			ValueLayer,
			PaintTransformCursor(AllottedGeometry, OutDrawElements, LayerId, Size));
	}

	const FVector2D LocalAxisPivot = AxisPivot * Size;
	const FVector2D LocalPivotPoint = PivotPoint * Size;
	const float Extent = Size.Size() * 2.0f;
	for (const FBlendViewTransformOverlayAxis& Axis : Axes)
	{
		const FVector2D Direction = (Axis.Direction * Size).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			{LocalAxisPivot - Direction * Extent, LocalAxisPivot + Direction * Extent},
			ESlateDrawEffect::None,
			Axis.Color,
			true,
			BlendViewTransformOverlay::GetAxisLineThicknessPixels());
	}

	int32 MaxLayerId = LayerId;
	if (GuideStart.IsSet() && GuideEnd.IsSet())
	{
		MaxLayerId = FMath::Max(
			MaxLayerId,
			BlendViewGuideDrawing::DrawSlateDashedGuide(
				OutDrawElements,
				AllottedGeometry,
				LayerId + 1,
				GuideStart.GetValue() * Size,
				GuideEnd.GetValue() * Size));
	}

	if (bDrawPivotMarker)
	{
		const float PivotScale = BlendViewTransformOverlay::GetDPIScale();
		const float PivotOuterRadius = PivotMarkerOuterRadiusPixels * PivotScale;
		const float PivotInnerRadius = PivotMarkerInnerRadiusPixels * PivotScale;
		const float PivotLineThickness = FMath::Max(1.0f, PivotScale);
		constexpr int32 PivotFillLines = 9;
		for (int32 Index = -PivotFillLines; Index <= PivotFillLines; ++Index)
		{
			const float Y = static_cast<float>(Index) / static_cast<float>(PivotFillLines);
			const float HalfWidth =
				FMath::Sqrt(FMath::Max(0.0f, 1.0f - Y * Y)) * PivotOuterRadius;
			const float InnerY = Y * PivotOuterRadius / PivotInnerRadius;
			const float InnerHalfWidth = FMath::Abs(InnerY) <= 1.0f
				? FMath::Sqrt(FMath::Max(0.0f, 1.0f - InnerY * InnerY)) * PivotInnerRadius
				: 0.0f;
			const FVector2D RowCenter =
				LocalPivotPoint + FVector2D(0.0f, Y * PivotOuterRadius);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 5,
				AllottedGeometry.ToPaintGeometry(),
				{RowCenter - FVector2D(HalfWidth, 0.0f),
					RowCenter + FVector2D(HalfWidth, 0.0f)},
				ESlateDrawEffect::None,
				FLinearColor::Black,
				true,
				PivotLineThickness);
			if (InnerHalfWidth > 0.0f)
			{
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId + 6,
					AllottedGeometry.ToPaintGeometry(),
					{RowCenter - FVector2D(InnerHalfWidth, 0.0f),
						RowCenter + FVector2D(InnerHalfWidth, 0.0f)},
					ESlateDrawEffect::None,
					SnapBaseMarkerColor,
					true,
					PivotLineThickness);
			}
		}
		MaxLayerId = FMath::Max(MaxLayerId, LayerId + 6);
	}

	if (SnapBaseMarker.IsSet())
	{
		const FVector2D Center = SnapBaseMarker.GetValue() * Size;
		const FVector2D HalfSize(
			SnapBaseMarkerHalfSizePixels,
			SnapBaseMarkerHalfSizePixels);
		TArray<FVector2D> MarkerPoints;
		if (SnapBaseMarkerKind == EBlendViewSnapTargetKind::Edge)
		{
			MarkerPoints = {
				Center + FVector2D(-HalfSize.X, -HalfSize.Y),
				Center + FVector2D(HalfSize.X, -HalfSize.Y),
				Center,
				Center + FVector2D(-HalfSize.X, HalfSize.Y),
				Center + FVector2D(HalfSize.X, HalfSize.Y),
				Center,
				Center + FVector2D(-HalfSize.X, -HalfSize.Y)};
		}
		else if (SnapBaseMarkerKind == EBlendViewSnapTargetKind::EdgeMidpoint)
		{
			MarkerPoints = {
				Center + FVector2D(0.0f, -HalfSize.Y),
				Center + FVector2D(HalfSize.X, HalfSize.Y),
				Center + FVector2D(-HalfSize.X, HalfSize.Y),
				Center + FVector2D(0.0f, -HalfSize.Y)};
		}
		else if (SnapBaseMarkerKind == EBlendViewSnapTargetKind::Face)
		{
			constexpr int32 CircleSegments = 24;
			MarkerPoints.Reserve(CircleSegments + 1);
			for (int32 Segment = 0; Segment <= CircleSegments; ++Segment)
			{
				const double Angle =
					(static_cast<double>(Segment) / CircleSegments) * 2.0 * PI;
				MarkerPoints.Add(
					Center +
					FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * HalfSize.X);
			}
		}
		else
		{
			MarkerPoints = {
				Center + FVector2D(-HalfSize.X, -HalfSize.Y),
				Center + FVector2D(HalfSize.X, -HalfSize.Y),
				Center + FVector2D(HalfSize.X, HalfSize.Y),
				Center + FVector2D(-HalfSize.X, HalfSize.Y),
				Center + FVector2D(-HalfSize.X, -HalfSize.Y)};
		}

		TArray<FVector2D> ShadowPoints;
		ShadowPoints.Reserve(MarkerPoints.Num());
		for (const FVector2D& Point : MarkerPoints)
		{
			ShadowPoints.Add(Point + FVector2D(1.0f, 1.0f));
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 3,
			AllottedGeometry.ToPaintGeometry(),
			ShadowPoints,
			ESlateDrawEffect::None,
			FLinearColor(0.0f, 0.0f, 0.0f, 0.65f),
			true,
			SnapBaseMarkerThicknessPixels);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 4,
			AllottedGeometry.ToPaintGeometry(),
			MarkerPoints,
			ESlateDrawEffect::None,
			SnapBaseMarkerColor,
			true,
			SnapBaseMarkerThicknessPixels);
		MaxLayerId = FMath::Max(MaxLayerId, LayerId + 4);
	}

	return FMath::Max(
		ValueLayer,
		PaintTransformCursor(
			AllottedGeometry,
			OutDrawElements,
			MaxLayerId,
			Size));
}

int32 SBlendViewTransformOverlay::PaintTransformValue(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FVector2D& Size) const
{
	if (ValueText.IsEmpty() || Size.X <= 1.0f || !FSlateApplication::IsInitialized())
	{
		return LayerId;
	}

	const FSlateFontInfo Font =
		FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
	const TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float LayoutScale = FMath::Max(
		AllottedGeometry.GetAccumulatedLayoutTransform().GetScale(),
		static_cast<float>(UE_SMALL_NUMBER));
	const FVector2D TextSize =
		FVector2D(FontMeasure->Measure(ValueText, Font, LayoutScale)) / LayoutScale;
	const FVector2D BoxSize(
		TextSize.X + TransformValueHorizontalPaddingPixels * 2.0f,
		TextSize.Y + TransformValueVerticalPaddingPixels * 2.0f);
	const FVector2D BoxPosition(
		FMath::Max((Size.X - BoxSize.X) * 0.5f, 0.0f),
		TransformValueTopInsetPixels);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(
			BoxSize,
			FSlateLayoutTransform(BoxPosition)),
		GetTransformValueBackgroundBrush(),
		ESlateDrawEffect::None,
		TransformValueBackgroundColor);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(
			TextSize,
			FSlateLayoutTransform(
				BoxPosition +
				FVector2D(
					TransformValueHorizontalPaddingPixels,
					TransformValueVerticalPaddingPixels))),
		ValueText,
		Font,
		ESlateDrawEffect::None,
		SceneTransformValueTextColor);
	return LayerId + 1;
}

int32 SBlendViewTransformOverlay::PaintTransformCursor(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FVector2D& Size) const
{
	if (!bHasCursorData || Size.X <= 1.0f || Size.Y <= 1.0f)
	{
		return LayerId;
	}

	return BlendViewTransformCursorRenderer::PaintSoftwareCursor(
		AllottedGeometry,
		OutDrawElements,
		LayerId,
		CursorMode,
		CursorPivotPoint * Size,
		CursorPoint * Size,
		bTrackballCursor);
}
