// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewPopupStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

namespace BlendViewPopupStyle
{
	TSharedRef<SWidget> MakeFloatingMenuShell(const TSharedRef<SWidget>& Content)
	{
		static const FSlateColorBrush BackgroundBrush(FStyleColors::Recessed);
		static const FSlateRoundedBoxBrush InnerOutlineBrush(
			FStyleColors::Recessed,
			2.0f,
			FStyleColors::InputOutline,
			1.0f);

		return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(&BackgroundBrush)
			]
			+ SOverlay::Slot()
			.Padding(1.0f)
			[
				SNew(SImage)
				.Image(&InnerOutlineBrush)
			]
			+ SOverlay::Slot()
			.Padding(2.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))
				.Padding(0.0f)
				[
					Content
				]
			];
	}

	TSharedRef<SWidget> MakeMenuContentShell(const TSharedRef<SWidget>& Content, const FMargin Padding)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))
			.Padding(Padding)
			[
				Content
			];
	}

	TSharedRef<SWidget> MakeMenuShell(const TSharedRef<SWidget>& Content, const FMargin Padding)
	{
		return MakeFloatingMenuShell(MakeMenuContentShell(Content, Padding));
	}

	TSharedRef<SWidget> MakeSeparator()
	{
		static const FSlateColorBrush SeparatorBrush(FLinearColor(FColor(0x32, 0x32, 0x32, 0xFF)));

		return SNew(SBox)
			.HeightOverride(1.0f)
			[
				SNew(SBorder)
				.BorderImage(&SeparatorBrush)
			];
	}
}
