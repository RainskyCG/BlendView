// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SGraphPanel;
class SOverlay;
class SWidget;
class SWindow;

enum class EBlendViewGraphOverlayHost : uint8
{
	Panel,
	Window
};

class FBlendViewGraphOverlayAttachment
{
public:
	bool Attach(
		const TSharedPtr<SGraphPanel>& Panel,
		const TSharedRef<SWidget>& Widget,
		int32 ZOrder,
		EBlendViewGraphOverlayHost PreferredHost);
	void Detach();
	void Abandon();

private:
	TWeakPtr<SOverlay> HostOverlay;
	TWeakPtr<SWindow> HostWindow;
	TSharedPtr<SWidget> AttachedWidget;
};
