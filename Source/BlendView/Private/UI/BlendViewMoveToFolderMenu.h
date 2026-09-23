// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FBlendViewViewportContext;

class FBlendViewMoveToFolderMenu
{
public:
	static bool Open(const FBlendViewViewportContext& ViewportContext, const FVector2D& ScreenPosition);
};
