// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneViewport;
class SBlendViewTransformOverlay;
class SLevelViewport;
class SOverlay;
class SWidget;

class FBlendViewTransformOverlaySession final
{
public:
	bool IsAttached() const;
	TSharedPtr<SBlendViewTransformOverlay> GetOverlay() const;
	TSharedPtr<SLevelViewport> GetLevelViewport() const;
	TSharedPtr<SWidget> GetOverlayWidget() const;

	void Attach(
		const TSharedPtr<SLevelViewport>& LevelViewport,
		const TSharedPtr<SWidget>& ViewportWidget,
		FSceneViewport* SceneViewport);
	void Detach();
	void Reset();

private:
	TSharedPtr<SWidget> OverlayWidget;
	TWeakPtr<SLevelViewport> OverlayLevelViewport;
	TWeakPtr<SOverlay> OverlayHost;
};
