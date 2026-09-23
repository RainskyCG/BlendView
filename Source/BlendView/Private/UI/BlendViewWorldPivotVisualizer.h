// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FBlendViewViewportContext;

class FBlendViewWorldPivotVisualizer final
{
public:
	static void Draw(
		const FBlendViewViewportContext& Context,
		const FVector& PivotLocation,
		const FLinearColor& PivotColor);

	static void Clear(const FBlendViewViewportContext& Context);
};
