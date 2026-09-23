// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Cursor/BlendViewSceneCursorRenderer.h"

#include "CanvasItem.h"
#include "Cursor/BlendViewSceneCursor.h"
#include "Engine/Canvas.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AxisDisplayInfo.h"
#include "Viewport/BlendViewViewportProjector.h"

namespace
{
	void DrawSoftViewportLine(
		UCanvas* Canvas,
		const FVector2D& Start,
		const FVector2D& End,
		const FLinearColor& Color,
		const float Thickness)
	{
		if (!Canvas || !Canvas->Canvas)
		{
			return;
		}

		const FVector2D Direction = End - Start;
		const double Length = Direction.Size();
		if (Length <= UE_SMALL_NUMBER)
		{
			return;
		}

		const FVector2D Perpendicular(-Direction.Y / Length, Direction.X / Length);
		const float SoftEdgeOffset = FMath::Clamp(Thickness * 0.45f, 0.3f, 0.9f);
		FLinearColor SoftEdgeColor = Color;
		SoftEdgeColor.A *= 0.18f;

		FCanvasLineItem SoftLineA(Start + Perpendicular * SoftEdgeOffset, End + Perpendicular * SoftEdgeOffset);
		SoftLineA.SetColor(SoftEdgeColor);
		SoftLineA.LineThickness = FMath::Max(Thickness * 0.5f, 0.5f);
		SoftLineA.Draw(Canvas->Canvas);

		FCanvasLineItem SoftLineB(Start - Perpendicular * SoftEdgeOffset, End - Perpendicular * SoftEdgeOffset);
		SoftLineB.SetColor(SoftEdgeColor);
		SoftLineB.LineThickness = FMath::Max(Thickness * 0.5f, 0.5f);
		SoftLineB.Draw(Canvas->Canvas);

		FCanvasLineItem LineItem(Start, End);
		LineItem.SetColor(Color);
		LineItem.LineThickness = Thickness;
		LineItem.Draw(Canvas->Canvas);
	}

	float GetUIScale()
	{
		return FSlateApplication::IsInitialized()
			? FMath::Max(FSlateApplication::Get().GetApplicationScale(), static_cast<float>(UE_SMALL_NUMBER))
			: 1;
	}
}

void FBlendViewSceneCursorRenderer::Draw(UCanvas* Canvas, const FBlendViewSceneCursor& Cursor)
{
	if (!Cursor.IsVisible() || !Canvas || !Canvas->Canvas)
	{
		return;
	}

	const FTransform& CursorTransform = Cursor.GetTransform();
	const FVector CursorLocation = CursorTransform.GetLocation();
	FVector2D Center;
	if (!FBlendViewViewportProjector::ProjectWorldToCanvas(Canvas, CursorLocation, Center))
	{
		return;
	}

	const float UIScale = GetUIScale();
	constexpr double AxisProbeWorldLength = 100;
	const double AxisScreenLength = 22 * UIScale;
	const double AxisScreenGap = 4 * UIScale;
	constexpr double AxisTipFraction = 0.2;
	const float AxisLineThickness = FMath::Max(0.65f * UIScale, 0.65f);
	const FLinearColor Red(0.95f, 0.08f, 0.08f, 1.0f);
	const FLinearColor White(0.95f, 0.95f, 0.92f, 1.0f);
	const FLinearColor AxisBodyColor(0.02f, 0.02f, 0.02f, 1.0f);
	const FLinearColor XAxisColor = AxisDisplayInfo::GetAxisColor(EAxisList::X);
	const FLinearColor YAxisColor = AxisDisplayInfo::GetAxisColor(EAxisList::Y);
	const FLinearColor ZAxisColor = AxisDisplayInfo::GetAxisColor(EAxisList::Z);

	auto DrawAxis = [Canvas, CursorLocation, &Center, AxisProbeWorldLength, AxisScreenLength, AxisScreenGap, AxisTipFraction, AxisLineThickness, &AxisBodyColor](
		const FVector& Axis,
		const FLinearColor& TipColor)
	{
		auto DrawHalfAxis = [Canvas, &Center, AxisScreenLength, AxisScreenGap, AxisTipFraction, AxisLineThickness, &AxisBodyColor, &TipColor](const FVector2D& ProbeEnd)
		{
			FVector2D Direction = ProbeEnd - Center;
			const double Length = Direction.Size();
			if (Length <= 1)
			{
				return;
			}

			Direction /= Length;
			const FVector2D Start = Center + Direction * AxisScreenGap;
			const FVector2D End = Center + Direction * AxisScreenLength;
			const FVector2D TipStart = End - Direction * ((AxisScreenLength - AxisScreenGap) * AxisTipFraction);
			DrawSoftViewportLine(Canvas, Start, TipStart, AxisBodyColor, AxisLineThickness);
			DrawSoftViewportLine(Canvas, TipStart, End, TipColor, AxisLineThickness);
		};

		FVector2D PositiveEnd;
		if (FBlendViewViewportProjector::ProjectWorldToCanvas(
				Canvas,
				CursorLocation + Axis * AxisProbeWorldLength,
				PositiveEnd))
		{
			DrawHalfAxis(PositiveEnd);
		}

		FVector2D NegativeEnd;
		if (FBlendViewViewportProjector::ProjectWorldToCanvas(
				Canvas,
				CursorLocation - Axis * AxisProbeWorldLength,
				NegativeEnd))
		{
			DrawHalfAxis(NegativeEnd);
		}
	};

	DrawAxis(CursorTransform.GetUnitAxis(EAxis::X), XAxisColor);
	DrawAxis(CursorTransform.GetUnitAxis(EAxis::Y), YAxisColor);
	DrawAxis(CursorTransform.GetUnitAxis(EAxis::Z), ZAxisColor);

	constexpr int32 RingSegments = 12;
	const double RingRadius = 11 * UIScale;
	constexpr double SegmentGapRadians = 0.035;
	constexpr int32 SubSteps = 10;
	const float RingLineThickness = FMath::Max(1 * UIScale, 1);
	for (int32 SegmentIndex = 0; SegmentIndex < RingSegments; ++SegmentIndex)
	{
		const double StartAngle =
			(2 * UE_DOUBLE_PI * static_cast<double>(SegmentIndex) / static_cast<double>(RingSegments)) +
			SegmentGapRadians;
		const double EndAngle =
			(2 * UE_DOUBLE_PI * static_cast<double>(SegmentIndex + 1) / static_cast<double>(RingSegments)) -
			SegmentGapRadians;
		const FLinearColor SegmentColor = (SegmentIndex % 2 == 0) ? White : Red;
		FVector2D PreviousPoint = Center + FVector2D(
			FMath::Cos(StartAngle) * RingRadius,
			FMath::Sin(StartAngle) * RingRadius);
		for (int32 Step = 1; Step <= SubSteps; ++Step)
		{
			const double Alpha = static_cast<double>(Step) / static_cast<double>(SubSteps);
			const double Angle = FMath::Lerp(StartAngle, EndAngle, Alpha);
			const FVector2D Point = Center + FVector2D(
				FMath::Cos(Angle) * RingRadius,
				FMath::Sin(Angle) * RingRadius);
			DrawSoftViewportLine(Canvas, PreviousPoint, Point, SegmentColor, RingLineThickness);
			PreviousPoint = Point;
		}
	}
}
