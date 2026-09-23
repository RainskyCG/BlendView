// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneViewport;
class SOverlay;
class SWidget;

class FBlendViewViewportOverlayLocator final
{
public:
	static TSharedPtr<SOverlay> FindForViewport(
		const TSharedPtr<SWidget>& Widget,
		const FSceneViewport* TargetViewport);
};
