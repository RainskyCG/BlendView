// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/SBlendViewShortcutCapturePopup.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandInfo.h"
#include "InputCoreTypes.h"
#include "Localization/BlendViewLocalization.h"
#include "Styling/AppStyle.h"
#include "UI/BlendViewPopupStyle.h"
#include "UI/BlendViewShortcutBinding.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlendViewShortcutCapturePopup::Construct(const FArguments& InArgs)
{
	CommandInfo = InArgs._CommandInfo;
	OnShortcutChanged = InArgs._OnShortcutChanged;
	bChangingExistingShortcut = InArgs._bChangingExistingShortcut;

	ChildSlot
	[
		BlendViewPopupStyle::MakeMenuContentShell(
			SNew(SBox)
			.WidthOverride(300.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
				[
					SNew(STextBlock)
					.Text(this, &SBlendViewShortcutCapturePopup::GetTitleText)
					.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 7.0f))
				[
					SNew(SBox)
					.HeightOverride(1.0f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush(TEXT("Menu.Separator")))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 5.0f))
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("EditableTextBox.Background.Focused")))
					.Padding(FMargin(8.0f, 6.0f))
					[
						SNew(STextBlock)
						.Text(this, &SBlendViewShortcutCapturePopup::GetShortcutText)
						.ColorAndOpacity(this, &SBlendViewShortcutCapturePopup::GetShortcutTextColor)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &SBlendViewShortcutCapturePopup::GetMessageText)
					.TextStyle(FAppStyle::Get(), TEXT("SmallText"))
					.ColorAndOpacity(FLinearColor(1.0f, 0.38f, 0.22f, 1.0f))
					.Visibility(this, &SBlendViewShortcutCapturePopup::GetMessageVisibility)
				]
			],
			FMargin(10.0f, 8.0f))
	];

	RegisterActiveTimer(
		0.0f,
		FWidgetActiveTimerDelegate::CreateSP(this, &SBlendViewShortcutCapturePopup::FocusSelf));
}

bool SBlendViewShortcutCapturePopup::SupportsKeyboardFocus() const
{
	return true;
}

FReply SBlendViewShortcutCapturePopup::OnKeyDown(const FGeometry&, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().DismissMenuByWidget(AsShared());
		}
		return FReply::Handled();
	}

	LastChord = FInputChord(
		Key.IsModifierKey() ? EKeys::Invalid : Key,
		KeyEvent.IsShiftDown(),
		KeyEvent.IsControlDown(),
		KeyEvent.IsAltDown(),
		KeyEvent.IsCommandDown());

	if (!LastChord.IsValidChord())
	{
		MessageText = FBlendViewLocalization::Text(
			TEXT("请按下包含非修饰键的快捷键。"),
			TEXT("Press a shortcut that includes a non-modifier key."));
		return FReply::Handled();
	}

	if (CommandInfo.IsValid())
	{
		FText ErrorText;
		if (BlendViewShortcutBinding::AssignPrimaryShortcut(CommandInfo.ToSharedRef(), LastChord, ErrorText))
		{
			OnShortcutChanged.ExecuteIfBound();
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().DismissMenuByWidget(AsShared());
			}
		}
		else
		{
			MessageText = ErrorText;
		}
	}

	return FReply::Handled();
}

EActiveTimerReturnType SBlendViewShortcutCapturePopup::FocusSelf(double, float)
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);
	}
	return EActiveTimerReturnType::Stop;
}

FText SBlendViewShortcutCapturePopup::GetTitleText() const
{
	return bChangingExistingShortcut
		? FBlendViewLocalization::Text(TEXT("改变快捷键"), TEXT("Change Shortcut"))
		: FBlendViewLocalization::Text(TEXT("指定快捷键"), TEXT("Assign Shortcut"));
}

FText SBlendViewShortcutCapturePopup::GetShortcutText() const
{
	return LastChord.IsValidChord()
		? LastChord.GetInputText()
		: FBlendViewLocalization::Text(TEXT("按下快捷键"), TEXT("Press shortcut"));
}

FSlateColor SBlendViewShortcutCapturePopup::GetShortcutTextColor() const
{
	return LastChord.IsValidChord()
		? FSlateColor::UseForeground()
		: FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f, 1.0f));
}

FText SBlendViewShortcutCapturePopup::GetMessageText() const
{
	return MessageText;
}

EVisibility SBlendViewShortcutCapturePopup::GetMessageVisibility() const
{
	return MessageText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}
