// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UCanvas;

namespace BlendViewTransformCanvasDrawing
{
	float GetGuideShadowOffsetPixels();
	float GetGuideShadowThicknessPixels();
	float GetGuideLineThicknessPixels();

	void DrawLine(
		UCanvas* Canvas,
		const FVector2D& Start,
		const FVector2D& End,
		const FLinearColor& Color,
		float Thickness,
		bool bPixelSnap = true);

	void DrawDashedLine(
		UCanvas* Canvas,
		const FVector2D& Start,
		const FVector2D& End,
		const FLinearColor& Color,
		float Thickness);
}
