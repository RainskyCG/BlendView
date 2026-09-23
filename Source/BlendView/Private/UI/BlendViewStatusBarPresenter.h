// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/BlendViewTransformTool.h"

class SWidget;
class SWindow;

class FBlendViewStatusBarPresenter final
{
public:
	static FBlendViewStatusBarPresenter& Get();

	TSharedRef<SWidget> CreateFallbackWidget();
	void Present(
		const TOptional<FBlendViewStatusLine>& StatusLine,
		const TSharedPtr<SWidget>& HostWidget = nullptr);
	void Restore();
	void PrepareForEngineExit();

private:
	bool TryPresentOnLeft(
		const FBlendViewStatusLine& StatusLine,
		const TSharedPtr<SWidget>& HostWidget);
	void SetFallbackLine(const TOptional<FBlendViewStatusLine>& StatusLine);

	TArray<TWeakPtr<SWidget>> FallbackWidgets;
	TWeakPtr<SWindow> OverlayHostWindow;
	TSharedPtr<SWidget> OverlayWidget;
};
