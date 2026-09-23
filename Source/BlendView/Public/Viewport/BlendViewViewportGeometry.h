// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;

class FBlendViewViewportGeometry final
{
public:
	static bool TryScreenToViewportPosition(
		const TSharedPtr<SWidget>& ViewportWidget,
		const FVector2D& ScreenPosition,
		const FIntPoint& ViewportSize,
		FVector2D& OutViewportPosition);
};
