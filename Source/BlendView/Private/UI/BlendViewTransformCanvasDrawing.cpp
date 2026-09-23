// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewTransformCanvasDrawing.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"

namespace
{
	constexpr double GuideDashLengthPixels = 6.0;
	constexpr double GuideGapLengthPixels = 5.0;
	constexpr float GuideShadowOffsetPixels = 1.0f;
	constexpr float GuideShadowThicknessPixels = 1.0f;
	constexpr float GuideLineThicknessPixels = 1.0f;
}

float BlendViewTransformCanvasDrawing::GetGuideShadowOffsetPixels()
{
	return GuideShadowOffsetPixels;
}

float BlendViewTransformCanvasDrawing::GetGuideShadowThicknessPixels()
{
	return GuideShadowThicknessPixels;
}

float BlendViewTransformCanvasDrawing::GetGuideLineThicknessPixels()
{
	return GuideLineThicknessPixels;
}

void BlendViewTransformCanvasDrawing::DrawLine(
	UCanvas* Canvas,
	const FVector2D& Start,
	const FVector2D& End,
	const FLinearColor& Color,
	const float Thickness,
	const bool bPixelSnap)
{
	if (!Canvas || !Canvas->Canvas)
	{
		return;
	}

	const FVector2D LineStart = bPixelSnap
		? FVector2D(FMath::RoundToDouble(Start.X) + 0.5, FMath::RoundToDouble(Start.Y) + 0.5)
		: Start;
	const FVector2D LineEnd = bPixelSnap
		? FVector2D(FMath::RoundToDouble(End.X) + 0.5, FMath::RoundToDouble(End.Y) + 0.5)
		: End;

	FCanvasLineItem LineItem(LineStart, LineEnd);
	LineItem.SetColor(Color);
	LineItem.LineThickness = Thickness;
	LineItem.Draw(Canvas->Canvas);
}

void BlendViewTransformCanvasDrawing::DrawDashedLine(
	UCanvas* Canvas,
	const FVector2D& Start,
	const FVector2D& End,
	const FLinearColor& Color,
	const float Thickness)
{
	const FVector2D Delta = End - Start;
	const double Length = Delta.Size();
	if (Length < 1.0)
	{
		return;
	}

	const FVector2D Direction = Delta / Length;
	for (double Offset = 0.0; Offset < Length; Offset += GuideDashLengthPixels + GuideGapLengthPixels)
	{
		const double SegmentEnd = FMath::Min(Offset + GuideDashLengthPixels, Length);
		DrawLine(Canvas, Start + Direction * Offset, Start + Direction * SegmentEnd, Color, Thickness, false);
	}
}
