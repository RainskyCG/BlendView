// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewVirtualPointerTracker.h"

void FBlendViewVirtualPointerTracker::Reset(const FVector2D& ViewportPosition)
{
	Position = ViewportPosition;
}

FVector2D FBlendViewVirtualPointerTracker::Advance(
	const FVector2D& CursorDelta,
	const double MouseScale)
{
	Position += CursorDelta * MouseScale;
	return Position;
}
