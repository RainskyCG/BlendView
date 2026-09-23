// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Snap/BlendViewSnapSolver.h"
#include "Tools/BlendViewTransformTypes.h"
#include "Widgets/SLeafWidget.h"

struct FBlendViewTransformOverlayAxis
{
	FVector2D Direction = FVector2D::ZeroVector;
	FLinearColor Color = FLinearColor::White;
};

namespace BlendViewTransformOverlay
{
	float GetDPIScale();
	float GetAxisLineThicknessPixels();
}

class SBlendViewTransformOverlay final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendViewTransformOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}

	void SetDrawingData(
		const FVector2D& InAxisPivot,
		const FVector2D& InPivotPoint,
		const TArray<FBlendViewTransformOverlayAxis>& InAxes,
		const TOptional<FVector2D>& InGuideStart,
		const TOptional<FVector2D>& InGuideEnd,
		const TOptional<FVector2D>& InSnapBaseMarker,
		EBlendViewSnapTargetKind InSnapBaseMarkerKind,
		const FLinearColor& InSnapBaseMarkerColor,
		bool bInDrawPivotMarker = true);
	void SetCursorData(
		EBlendViewTransformMode InMode,
		const FVector2D& InPivotPoint,
		const FVector2D& InCursorPoint,
		bool bInTrackball);
	void ClearCursorData();
	void ClearDrawingData();
	void SetValueText(const FText& InValueText);

private:
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	int32 PaintTransformValue(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& Size) const;
	int32 PaintTransformCursor(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& Size) const;

	FVector2D AxisPivot = FVector2D::ZeroVector;
	FVector2D PivotPoint = FVector2D::ZeroVector;
	FVector2D CursorPivotPoint = FVector2D::ZeroVector;
	FVector2D CursorPoint = FVector2D::ZeroVector;
	TArray<FBlendViewTransformOverlayAxis> Axes;
	TOptional<FVector2D> GuideStart;
	TOptional<FVector2D> GuideEnd;
	TOptional<FVector2D> SnapBaseMarker;
	EBlendViewSnapTargetKind SnapBaseMarkerKind = EBlendViewSnapTargetKind::None;
	EBlendViewTransformMode CursorMode = EBlendViewTransformMode::Translate;
	FLinearColor SnapBaseMarkerColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);
	FText ValueText;
	bool bHasDrawingData = false;
	bool bHasCursorData = false;
	bool bTrackballCursor = false;
	bool bDrawPivotMarker = true;
};
