// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneView;
class FSceneViewFamilyContext;
class UCanvas;
struct FBlendViewViewportContext;

class FBlendViewViewportProjector final
{
public:
	static FSceneView* CreateSceneView(
		const FBlendViewViewportContext& Context,
		FSceneViewFamilyContext& ViewFamily);
	static bool ProjectWorldToViewport(
		const FBlendViewViewportContext& Context,
		const FVector& WorldPosition,
		FVector2D& OutViewportPosition);
	static bool GetViewportRay(
		const FBlendViewViewportContext& Context,
		const FVector2D& ViewportPosition,
		FVector& OutRayStart,
		FVector& OutRayDirection);
	static bool GetViewportTraceSegment(
		const FBlendViewViewportContext& Context,
		const FVector2D& ViewportPosition,
		FVector& OutTraceStart,
		FVector& OutTraceEnd);
	static bool GetViewBasis(
		const FBlendViewViewportContext& Context,
		FVector& OutViewForward,
		FVector& OutViewRight,
		FVector& OutViewUp);
	static bool ProjectWorldToCanvas(
		UCanvas* Canvas,
		const FVector& WorldPosition,
		FVector2D& OutCanvasPosition);
	static bool IsCanvasForViewport(
		const FBlendViewViewportContext& Context,
		UCanvas* Canvas);
	static bool ProjectWorldToOverlay(
		const FBlendViewViewportContext& Context,
		const FVector& WorldPosition,
		FVector2D& OutNormalizedPosition);
};
