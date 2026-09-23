// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/BlendViewTransformTypes.h"

class FSlateWindowElementList;
struct FGeometry;

namespace BlendViewTransformCursorRenderer
{
	int32 PaintSoftwareCursor(
		const FGeometry& Geometry,
		FSlateWindowElementList& DrawElements,
		int32 LayerId,
		EBlendViewTransformMode Mode,
		const FVector2D& Pivot,
		const FVector2D& Cursor,
		bool bTrackball = false);
}
