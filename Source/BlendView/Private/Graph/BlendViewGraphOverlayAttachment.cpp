// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Graph/BlendViewGraphOverlayAttachment.h"

#include "Graph/BlendViewGraphContextResolver.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"

bool FBlendViewGraphOverlayAttachment::Attach(
	const TSharedPtr<SGraphPanel>& Panel,
	const TSharedRef<SWidget>& Widget,
	const int32 ZOrder,
	const EBlendViewGraphOverlayHost PreferredHost)
{
	Detach();

	if (PreferredHost == EBlendViewGraphOverlayHost::Panel)
	{
		if (const TSharedPtr<SOverlay> Overlay =
			FBlendViewGraphContextResolver::FindHostOverlay(Panel))
		{
			Overlay->AddSlot(ZOrder)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					Widget
				];
			HostOverlay = Overlay;
			AttachedWidget = Widget;
			return true;
		}
	}

	if (const TSharedPtr<SWindow> Window =
		FBlendViewGraphContextResolver::FindHostWindow(Panel))
	{
		Window->AddOverlaySlot(ZOrder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				Widget
			];
		HostWindow = Window;
		AttachedWidget = Widget;
		return true;
	}
	return false;
}

void FBlendViewGraphOverlayAttachment::Detach()
{
	if (AttachedWidget.IsValid())
	{
		if (const TSharedPtr<SOverlay> Overlay = HostOverlay.Pin())
		{
			Overlay->RemoveSlot(AttachedWidget.ToSharedRef());
		}
		if (const TSharedPtr<SWindow> Window = HostWindow.Pin())
		{
			Window->RemoveOverlaySlot(AttachedWidget.ToSharedRef());
		}
	}
	Abandon();
}

void FBlendViewGraphOverlayAttachment::Abandon()
{
	AttachedWidget.Reset();
	HostOverlay.Reset();
	HostWindow.Reset();
}
