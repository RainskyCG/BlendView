// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewTransformOverlaySession.h"

#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Slate/SceneViewport.h"
#include "UI/BlendViewTransformOverlay.h"
#include "UI/BlendViewViewportOverlayLocator.h"
#include "Widgets/SOverlay.h"

bool FBlendViewTransformOverlaySession::IsAttached() const
{
	return OverlayWidget.IsValid();
}

TSharedPtr<SBlendViewTransformOverlay> FBlendViewTransformOverlaySession::GetOverlay() const
{
	return StaticCastSharedPtr<SBlendViewTransformOverlay>(OverlayWidget);
}

TSharedPtr<SLevelViewport> FBlendViewTransformOverlaySession::GetLevelViewport() const
{
	return OverlayLevelViewport.Pin();
}

TSharedPtr<SWidget> FBlendViewTransformOverlaySession::GetOverlayWidget() const
{
	return OverlayWidget;
}

void FBlendViewTransformOverlaySession::Attach(
	const TSharedPtr<SLevelViewport>& LevelViewport,
	const TSharedPtr<SWidget>& ViewportWidget,
	FSceneViewport* SceneViewport)
{
	if (OverlayWidget.IsValid())
	{
		return;
	}

	TSharedPtr<SBlendViewTransformOverlay> Overlay;
	SAssignNew(Overlay, SBlendViewTransformOverlay)
		.Visibility(EVisibility::HitTestInvisible);

	if (LevelViewport.IsValid())
	{
		OverlayWidget = Overlay;
		OverlayLevelViewport = LevelViewport;
		LevelViewport->AddOverlayWidget(Overlay.ToSharedRef(), 100);
		return;
	}

	const TSharedPtr<SOverlay> HostOverlay =
		FBlendViewViewportOverlayLocator::FindForViewport(ViewportWidget, SceneViewport);
	if (!HostOverlay.IsValid())
	{
		return;
	}

	OverlayWidget = Overlay;
	OverlayHost = HostOverlay;
	HostOverlay->AddSlot(100)
	[
		Overlay.ToSharedRef()
	];
}

void FBlendViewTransformOverlaySession::Detach()
{
	if (TSharedPtr<SLevelViewport> LevelViewport = OverlayLevelViewport.Pin())
	{
		if (OverlayWidget.IsValid())
		{
			LevelViewport->RemoveOverlayWidget(OverlayWidget.ToSharedRef());
		}
	}
	else if (const TSharedPtr<SOverlay> HostOverlay = OverlayHost.Pin())
	{
		if (OverlayWidget.IsValid())
		{
			HostOverlay->RemoveSlot(OverlayWidget.ToSharedRef());
		}
	}
	Reset();
}

void FBlendViewTransformOverlaySession::Reset()
{
	OverlayWidget.Reset();
	OverlayLevelViewport.Reset();
	OverlayHost.Reset();
}
