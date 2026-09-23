// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewSceneCursor;
struct FBlendViewViewportContext;

enum class EBlendViewSceneCursorOrientationMode : uint8
{
	World,
	View,
	Surface
};

struct FBlendViewSceneCursorPlacementOptions
{
	EBlendViewSceneCursorOrientationMode OrientationMode = EBlendViewSceneCursorOrientationMode::Surface;
	bool bSurfaceProject = true;
};

struct FBlendViewSceneCursorPlacementResult
{
	FTransform Transform = FTransform::Identity;
	bool bHitSurface = false;
	bool bUpdatedRotation = false;
};

class FBlendViewSceneCursorPlacement final
{
public:
	static bool Resolve(
		const FBlendViewViewportContext& Context,
		const FVector2D& ViewportPosition,
		const FBlendViewSceneCursor& CurrentCursor,
		const FBlendViewSceneCursorPlacementOptions& Options,
		FBlendViewSceneCursorPlacementResult& OutResult);
};
