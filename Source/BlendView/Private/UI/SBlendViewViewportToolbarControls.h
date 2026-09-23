// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SMenuAnchor.h"

class SBlendViewPivotMenuButton;
class SBlendViewSnapSplitButton;
struct FButtonStyle;
struct FComboButtonStyle;
struct FSlateBrush;

DECLARE_DELEGATE_RetVal(bool, FBlendViewIsPersistentSnapEnabled);

class SBlendViewViewportToolbarControls final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendViewViewportToolbarControls)
		{}
		SLATE_STYLE_ARGUMENT(FComboButtonStyle, ComboButtonStyle)
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
		SLATE_ATTRIBUTE(const FSlateBrush*, PivotIcon)
		SLATE_ATTRIBUTE(FText, PivotToolTip)
		SLATE_ATTRIBUTE(const FSlateBrush*, SnapIcon)
		SLATE_ATTRIBUTE(FText, SnapToolTip)
		SLATE_ATTRIBUTE(FText, SnapTargetLabel)
		SLATE_EVENT(FOnGetContent, OnGetPivotMenuContent)
		SLATE_EVENT(FOnGetContent, OnGetSnapMenuContent)
		SLATE_EVENT(FSimpleDelegate, OnTogglePersistentSnap)
		SLATE_EVENT(FBlendViewIsPersistentSnapEnabled, IsPersistentSnapEnabled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SBlendViewViewportToolbarControls() override;


private:
	enum class EMenuKind : uint8
	{
		Pivot,
		Snap
	};

	TSharedRef<SWidget> HandleGetPivotMenuContent();
	TSharedRef<SWidget> HandleGetSnapMenuContent();
	void HandleMenuOpenChanged(bool bIsOpen, EMenuKind MenuKind);
	void CloseMenu(EMenuKind MenuKind);

	TSharedPtr<SBlendViewPivotMenuButton> PivotMenuButton;
	TSharedPtr<SBlendViewSnapSplitButton> SnapSplitButton;
	TSharedPtr<SWidget> PivotMenuContent;
	TSharedPtr<SWidget> SnapMenuContent;
	FOnGetContent OnGetPivotMenuContent;
	FOnGetContent OnGetSnapMenuContent;
};
