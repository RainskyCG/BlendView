// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewVirtualPointerTracker
{
public:
	void Reset(const FVector2D& ViewportPosition);
	FVector2D Advance(const FVector2D& CursorDelta, double MouseScale);

	const FVector2D& GetPosition() const { return Position; }

private:
	FVector2D Position = FVector2D::ZeroVector;
};
