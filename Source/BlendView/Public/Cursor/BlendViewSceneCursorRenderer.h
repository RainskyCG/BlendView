// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewSceneCursor;
class UCanvas;

class FBlendViewSceneCursorRenderer final
{
public:
	static void Draw(UCanvas* Canvas, const FBlendViewSceneCursor& Cursor);
};
