// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Viewport/BlendViewViewportGeometry.h"

#include "Layout/Geometry.h"
#include "Widgets/SWidget.h"

bool FBlendViewViewportGeometry::TryScreenToViewportPosition(
	const TSharedPtr<SWidget>& ViewportWidget,
	const FVector2D& ScreenPosition,
	const FIntPoint& ViewportSize,
	FVector2D& OutViewportPosition)
{
	if (!ViewportWidget.IsValid() || ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return false;
	}

	const FGeometry& ViewportGeometry = ViewportWidget->GetCachedGeometry();
	const FVector2D LocalSize = ViewportGeometry.GetLocalSize();
	if (LocalSize.X <= UE_KINDA_SMALL_NUMBER || LocalSize.Y <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector2D LocalPosition = ViewportGeometry.AbsoluteToLocal(ScreenPosition);
	if (LocalPosition.X < 0.0 || LocalPosition.X > LocalSize.X ||
		LocalPosition.Y < 0.0 || LocalPosition.Y > LocalSize.Y)
	{
		return false;
	}

	OutViewportPosition = FVector2D(
		LocalPosition.X * static_cast<double>(ViewportSize.X) / LocalSize.X,
		LocalPosition.Y * static_cast<double>(ViewportSize.Y) / LocalSize.Y);
	return true;
}
