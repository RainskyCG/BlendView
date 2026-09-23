// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewSceneCursor;

class FBlendViewSceneCursorState final
{
public:
	static bool TryGetTransform(const FBlendViewSceneCursor& SceneCursor, FTransform& OutTransform);
	static bool TryGetSavedTransform(FTransform& OutTransform);
	static void SetTransform(FBlendViewSceneCursor& SceneCursor, const FTransform& Transform);
	static void SetLocationPreservingRotation(FBlendViewSceneCursor& SceneCursor, const FVector& Location);
};
