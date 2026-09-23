// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewViewportOverlayLocator.h"

#include "Slate/SceneViewport.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"

namespace
{
	TSharedPtr<SOverlay> FindDescendantOverlay(const TSharedPtr<SWidget>& Widget)
	{
		if (!Widget.IsValid())
		{
			return nullptr;
		}

		if (Widget->GetType() == TEXT("SOverlay"))
		{
			return StaticCastSharedPtr<SOverlay>(Widget);
		}

		FChildren* Children = Widget->GetAllChildren();
		if (!Children)
		{
			return nullptr;
		}

		for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			const TSharedRef<SWidget> Child = Children->GetChildAt(ChildIndex);
			if (const TSharedPtr<SOverlay> Overlay = FindDescendantOverlay(Child))
			{
				return Overlay;
			}
		}

		return nullptr;
	}
}

TSharedPtr<SOverlay> FBlendViewViewportOverlayLocator::FindForViewport(
	const TSharedPtr<SWidget>& Widget,
	const FSceneViewport* TargetViewport)
{
	if (!Widget.IsValid() || !TargetViewport)
	{
		return nullptr;
	}

	if (Widget->GetType() == TEXT("SViewport"))
	{
		const TSharedPtr<SViewport> SlateViewportWidget = StaticCastSharedPtr<SViewport>(Widget);
		if (TargetViewport->GetViewportWidget().Pin().Get() == SlateViewportWidget.Get())
		{
			return FindDescendantOverlay(SlateViewportWidget->GetContent());
		}
	}

	FChildren* Children = Widget->GetAllChildren();
	if (!Children)
	{
		return nullptr;
	}

	for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
	{
		const TSharedRef<SWidget> Child = Children->GetChildAt(ChildIndex);
		if (const TSharedPtr<SOverlay> Overlay = FindForViewport(Child, TargetViewport))
		{
			return Overlay;
		}
	}

	return nullptr;
}
