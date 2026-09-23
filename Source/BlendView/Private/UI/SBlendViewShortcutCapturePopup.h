// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputChord.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandInfo;

class SBlendViewShortcutCapturePopup final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendViewShortcutCapturePopup) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandInfo>, CommandInfo)
		SLATE_ARGUMENT(bool, bChangingExistingShortcut)
		SLATE_EVENT(FSimpleDelegate, OnShortcutChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;

private:
	EActiveTimerReturnType FocusSelf(double CurrentTime, float DeltaTime);
	FText GetTitleText() const;
	FText GetShortcutText() const;
	FSlateColor GetShortcutTextColor() const;
	FText GetMessageText() const;
	EVisibility GetMessageVisibility() const;

	TSharedPtr<FUICommandInfo> CommandInfo;
	FSimpleDelegate OnShortcutChanged;
	FInputChord LastChord;
	FText MessageText;
	bool bChangingExistingShortcut = false;
};
