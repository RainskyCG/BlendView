// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewStatusBarPresenter.h"

#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float StatusBarHeightPixels = 28.0f;
	constexpr float StatusBarLeftInsetPixels = 0.0f;
	constexpr float StatusHintGapPixels = 14.0f;
	constexpr float StatusTokenGroupGapPixels = 2.0f;
	constexpr float StatusShiftMouseGapPixels = -3.0f;
	constexpr float StatusTokenLabelGapPixels = 1.0f;
	constexpr float StatusMouseLabelGapPixels = -3.0f;
	constexpr float StatusKeycapExtentPixels = 21.0f;
	constexpr float StatusInputExtentPixels = 21.0f;

	const FLinearColor& GetStatusTextColor()
	{
		static const FLinearColor Color = FLinearColor::FromSRGBColor(FColor(0xC5, 0xC5, 0xC5, 0xFF));
		return Color;
	}

	FString GetBlendViewResourcesDir()
	{
		static FString ResourcesDir;
		if (ResourcesDir.IsEmpty())
		{
			if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlendView")))
			{
				ResourcesDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"));
			}
			else
			{
				ResourcesDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("BlendView"), TEXT("Resources"));
			}
		}

		return ResourcesDir;
	}

	FString GetStatusTokenIconFileName(const FBlendViewStatusToken& Token)
	{
		const FString TokenText = Token.Text.ToString();
		if (Token.Kind == EBlendViewStatusTokenKind::Mouse)
		{
			if (TokenText.Equals(TEXT("LMB"), ESearchCase::IgnoreCase))
			{
				return TEXT("MOUSE_LMB.svg");
			}
			if (TokenText.Equals(TEXT("MMB"), ESearchCase::IgnoreCase))
			{
				return TEXT("MOUSE_MMB.svg");
			}
			if (TokenText.Equals(TEXT("RMB"), ESearchCase::IgnoreCase))
			{
				return TEXT("MOUSE_RMB.svg");
			}
		}
		else if (TokenText.Equals(TEXT("Shift"), ESearchCase::IgnoreCase))
		{
			return TEXT("KEY_SHIFT.svg");
		}
		else if (TokenText.Equals(TEXT("Tab"), ESearchCase::IgnoreCase))
		{
			return TEXT("KEY_TAB_INVERTED.svg");
		}

		return FString();
	}

	FVector2D GetStatusTokenIconSize(const FBlendViewStatusToken& Token)
	{
		return FVector2D(StatusInputExtentPixels, StatusInputExtentPixels);
	}

	bool HasStatusTokenIcon(const FBlendViewStatusToken& Token)
	{
		const FString FileName = GetStatusTokenIconFileName(Token);
		return !FileName.IsEmpty() && FPaths::FileExists(FPaths::Combine(GetBlendViewResourcesDir(), FileName));
	}

	const FSlateBrush* GetStatusTokenIconBrush(const FBlendViewStatusToken& Token, const FVector2D& IconSize)
	{
		const FString FileName = GetStatusTokenIconFileName(Token);
		if (FileName.IsEmpty())
		{
			return nullptr;
		}

		const FString IconPath = FPaths::Combine(GetBlendViewResourcesDir(), FileName);
		if (!FPaths::FileExists(IconPath))
		{
			return nullptr;
		}

		static TMap<FString, TUniquePtr<FSlateVectorImageBrush>> Brushes;
		const FString BrushKey = FString::Printf(TEXT("%s|%.1f|%.1f"), *IconPath, IconSize.X, IconSize.Y);
		if (!Brushes.Contains(BrushKey))
		{
			Brushes.Add(
				BrushKey,
				MakeUnique<FSlateVectorImageBrush>(
					IconPath,
					FVector2f(static_cast<float>(IconSize.X), static_cast<float>(IconSize.Y)),
					FLinearColor(0.62f, 0.62f, 0.62f, 0.95f)));
		}

		const TUniquePtr<FSlateVectorImageBrush>* Brush = Brushes.Find(BrushKey);
		return Brush ? Brush->Get() : nullptr;
	}

	const FSlateBrush* GetStatusKeycapBrush()
	{
		static const FSlateRoundedBoxBrush Brush(
			FLinearColor::Transparent,
			4.0f,
			FLinearColor(0.82f, 0.82f, 0.82f, 0.95f),
			1.0f,
			FVector2f(StatusKeycapExtentPixels, StatusKeycapExtentPixels));
		return &Brush;
	}

	const FSlateBrush* GetStatusBarBackgroundBrush()
	{
		static const FSlateRoundedBoxBrush Brush(
			FLinearColor::FromSRGBColor(FColor(0x24, 0x24, 0x24, 0xFF)),
			5.0f,
			FVector2f(160.0f, StatusBarHeightPixels));
		return &Brush;
	}

	class SBlendViewStatusBarWidget final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlendViewStatusBarWidget) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&)
		{
			ChildSlot
			[
				SAssignNew(RootBox, SBox)
					.HeightOverride(StatusBarHeightPixels)
					[
						SNew(SBorder)
							.BorderImage(GetStatusBarBackgroundBrush())
							.BorderBackgroundColor(FLinearColor::White)
							.Padding(FMargin(6.0f, 1.0f))
							[
								SAssignNew(ContentBox, SHorizontalBox)
							]
					]
			];
			SetVisibility(EVisibility::Collapsed);
		}

		void SetStatusLine(const TOptional<FBlendViewStatusLine>& InStatusLine)
		{
			const FString NewSignature = MakeSignature(InStatusLine);
			if (NewSignature == Signature)
			{
				return;
			}

			Signature = NewSignature;
			ContentBox->ClearChildren();
			if (!InStatusLine.IsSet())
			{
				SetVisibility(EVisibility::Collapsed);
				return;
			}

			const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
			for (const FBlendViewStatusHint& Hint : InStatusLine.GetValue().Hints)
			{
				for (int32 TokenIndex = 0; TokenIndex < Hint.Tokens.Num(); ++TokenIndex)
				{
					const FBlendViewStatusToken& Token = Hint.Tokens[TokenIndex];
					const bool bHasFollowingToken = TokenIndex + 1 < Hint.Tokens.Num();
					const bool bShiftMousePair =
						bHasFollowingToken &&
						Token.Text.ToString().Equals(TEXT("Shift"), ESearchCase::IgnoreCase) &&
						Hint.Tokens[TokenIndex + 1].Kind == EBlendViewStatusTokenKind::Mouse;
					const float RightPadding = bShiftMousePair
						? StatusShiftMouseGapPixels
						: bHasFollowingToken
							? StatusTokenGroupGapPixels
							: Token.Kind == EBlendViewStatusTokenKind::Mouse
								? StatusMouseLabelGapPixels
								: StatusTokenLabelGapPixels;
					ContentBox->AddSlot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, RightPadding, 0.0f)
						[
							MakeTokenWidget(Token, FontInfo)
						];
				}

				ContentBox->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, StatusHintGapPixels, 0.0f)
					[
						SNew(STextBlock)
							.Font(FontInfo)
							.ColorAndOpacity(GetStatusTextColor())
							.Text(Hint.Label)
					];
			}

			SetVisibility(EVisibility::SelfHitTestInvisible);
			Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		}

		void SetFixedWidth(const float Width)
		{
			if (RootBox.IsValid())
			{
				RootBox->SetWidthOverride(FOptionalSize(Width));
				Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			}
		}

	private:
		static FString MakeSignature(const TOptional<FBlendViewStatusLine>& StatusLine)
		{
			if (!StatusLine.IsSet())
			{
				return FString();
			}

			FString Result;
			for (const FBlendViewStatusHint& Hint : StatusLine.GetValue().Hints)
			{
				for (const FBlendViewStatusToken& Token : Hint.Tokens)
				{
					Result += FString::Printf(TEXT("%d:%s|"), static_cast<int32>(Token.Kind), *Token.Text.ToString());
				}
				Result += Hint.Label.ToString();
				Result += TEXT(";");
			}
			return Result;
		}

		static TSharedRef<SWidget> MakeTokenWidget(
			const FBlendViewStatusToken& Token,
			const FSlateFontInfo& FontInfo)
		{
			if (HasStatusTokenIcon(Token))
			{
				const FVector2D IconSize = GetStatusTokenIconSize(Token);
				return SNew(SBox)
					.WidthOverride(StatusInputExtentPixels)
					.HeightOverride(StatusInputExtentPixels)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
							.WidthOverride(IconSize.X)
							.HeightOverride(IconSize.Y)
							[
								SNew(SImage)
									.Image(GetStatusTokenIconBrush(Token, IconSize))
							]
					];
			}

			const bool bSquareKeycap = Token.Text.ToString().Len() == 1;
			TSharedRef<SBorder> Keycap = SNew(SBorder)
				.BorderImage(GetStatusKeycapBrush())
				.BorderBackgroundColor(FLinearColor::White)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(FMargin(bSquareKeycap ? 0.0f : 5.0f, 0.0f))
				[
					SNew(STextBlock)
						.Font(FontInfo)
						.ColorAndOpacity(GetStatusTextColor())
						.Justification(ETextJustify::Center)
						.Text(Token.Text)
				];

			if (bSquareKeycap)
			{
				return SNew(SBox)
					.WidthOverride(StatusKeycapExtentPixels)
					.HeightOverride(StatusKeycapExtentPixels)
					[
						Keycap
					];
			}

			return SNew(SBox)
				.HeightOverride(StatusKeycapExtentPixels)
				[
					Keycap
				];
		}

		TSharedPtr<SBox> RootBox;
		TSharedPtr<SHorizontalBox> ContentBox;
		FString Signature;
	};

	TSharedPtr<SWidget> FindVisibleWidgetByType(const TSharedRef<SWidget>& Root, const FName TypeName)
	{
		if (!Root->GetVisibility().IsVisible())
		{
			return nullptr;
		}

		if (Root->GetType() == TypeName)
		{
			return Root;
		}

		FChildren* Children = Root->GetChildren();
		if (!Children)
		{
			return nullptr;
		}

		for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			if (TSharedPtr<SWidget> Match = FindVisibleWidgetByType(Children->GetChildAt(ChildIndex), TypeName))
			{
				return Match;
			}
		}

		return nullptr;
	}

	void SetWidgetLine(const TSharedPtr<SWidget>& Widget, const TOptional<FBlendViewStatusLine>& StatusLine)
	{
		if (Widget.IsValid())
		{
			StaticCastSharedPtr<SBlendViewStatusBarWidget>(Widget)->SetStatusLine(StatusLine);
		}
	}
}

FBlendViewStatusBarPresenter& FBlendViewStatusBarPresenter::Get()
{
	static FBlendViewStatusBarPresenter Presenter;
	return Presenter;
}

TSharedRef<SWidget> FBlendViewStatusBarPresenter::CreateFallbackWidget()
{
	TSharedRef<SBlendViewStatusBarWidget> Widget = SNew(SBlendViewStatusBarWidget);
	FallbackWidgets.Add(Widget);
	return Widget;
}

void FBlendViewStatusBarPresenter::Present(
	const TOptional<FBlendViewStatusLine>& StatusLine,
	const TSharedPtr<SWidget>& HostWidget)
{
	if (!StatusLine.IsSet())
	{
		Restore();
		return;
	}

	if (OverlayWidget.IsValid() && OverlayHostWindow.IsValid())
	{
		SetWidgetLine(OverlayWidget, StatusLine);
		SetFallbackLine(TOptional<FBlendViewStatusLine>());
		return;
	}

	Restore();
	if (!TryPresentOnLeft(StatusLine.GetValue(), HostWidget))
	{
		SetFallbackLine(StatusLine);
	}
}

void FBlendViewStatusBarPresenter::Restore()
{
	if (const TSharedPtr<SWindow> HostWindow = OverlayHostWindow.Pin())
	{
		if (OverlayWidget.IsValid())
		{
			HostWindow->RemoveOverlaySlot(OverlayWidget.ToSharedRef());
		}
	}

	OverlayHostWindow.Reset();
	OverlayWidget.Reset();
	SetFallbackLine(TOptional<FBlendViewStatusLine>());
}

void FBlendViewStatusBarPresenter::PrepareForEngineExit()
{
	OverlayHostWindow.Reset();
	OverlayWidget.Reset();
	FallbackWidgets.Reset();
}

bool FBlendViewStatusBarPresenter::TryPresentOnLeft(
	const FBlendViewStatusLine& StatusLine,
	const TSharedPtr<SWidget>& HostWidget)
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	TSharedPtr<SWindow> HostWindow;
	if (HostWidget.IsValid())
	{
		HostWindow = FSlateApplication::Get().FindWidgetWindow(HostWidget.ToSharedRef());
	}
	if (!HostWindow.IsValid())
	{
		HostWindow = FSlateApplication::Get().GetActiveTopLevelRegularWindow();
	}
	if (!HostWindow.IsValid())
	{
		return false;
	}

	const TSharedPtr<SWidget> StatusBar = FindVisibleWidgetByType(
		HostWindow->GetContent(),
		FName(TEXT("SStatusBar")));
	if (!StatusBar.IsValid())
	{
		if (!HostWidget.IsValid())
		{
			return false;
		}

		const FGeometry& WindowGeometry = HostWindow->GetCachedGeometry();
		const FGeometry& HostGeometry = HostWidget->GetCachedGeometry();
		const FVector2D HostTopLeft = WindowGeometry.AbsoluteToLocal(
			HostGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D HostBottomRight = WindowGeometry.AbsoluteToLocal(
			HostGeometry.LocalToAbsolute(HostGeometry.GetLocalSize()));
		const float HostWidth = HostBottomRight.X - HostTopLeft.X;
		if (HostWidth <= 0.0f)
		{
			return false;
		}

		TSharedRef<SBlendViewStatusBarWidget> BottomWidget = SNew(SBlendViewStatusBarWidget);
		BottomWidget->SetStatusLine(StatusLine);
		BottomWidget->SetFixedWidth(HostWidth);
		HostWindow->AddOverlaySlot(10000)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(
				FMath::Max(HostTopLeft.X, 0.0f),
				0.0f,
				0.0f,
				FMath::Max(WindowGeometry.GetLocalSize().Y - HostBottomRight.Y, 0.0f)))
			[
				BottomWidget
			];

		OverlayHostWindow = HostWindow;
		OverlayWidget = BottomWidget;
		SetFallbackLine(TOptional<FBlendViewStatusLine>());
		return true;
	}

	const FGeometry& WindowGeometry = HostWindow->GetCachedGeometry();
	const FGeometry& StatusBarGeometry = StatusBar->GetCachedGeometry();
	const FVector2D StatusBarTopLeft = WindowGeometry.AbsoluteToLocal(
		StatusBarGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D StatusBarBottomRight = WindowGeometry.AbsoluteToLocal(
		StatusBarGeometry.LocalToAbsolute(StatusBarGeometry.GetLocalSize()));
	const float NativeStatusBarHeight = StatusBarBottomRight.Y - StatusBarTopLeft.Y;
	const float NativeStatusBarWidth = StatusBarBottomRight.X - StatusBarTopLeft.X;
	if (NativeStatusBarHeight <= 0.0f)
	{
		return false;
	}

	const float LeftInset = FMath::Max(StatusBarTopLeft.X + StatusBarLeftInsetPixels, StatusBarLeftInsetPixels);
	const float BottomInset = FMath::Max(
		WindowGeometry.GetLocalSize().Y - StatusBarBottomRight.Y +
			(NativeStatusBarHeight - StatusBarHeightPixels) * 0.5f,
		0.0f);

	TSharedRef<SBlendViewStatusBarWidget> LeftWidget = SNew(SBlendViewStatusBarWidget);
	LeftWidget->SetStatusLine(StatusLine);
	LeftWidget->SetFixedWidth(NativeStatusBarWidth);
	HostWindow->AddOverlaySlot(10000)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(LeftInset, 0.0f, 0.0f, BottomInset))
		[
			LeftWidget
		];

	OverlayHostWindow = HostWindow;
	OverlayWidget = LeftWidget;
	SetFallbackLine(TOptional<FBlendViewStatusLine>());
	return true;
}

void FBlendViewStatusBarPresenter::SetFallbackLine(const TOptional<FBlendViewStatusLine>& StatusLine)
{
	FallbackWidgets.RemoveAll([](const TWeakPtr<SWidget>& Widget)
	{
		return !Widget.IsValid();
	});

	for (const TWeakPtr<SWidget>& WeakWidget : FallbackWidgets)
	{
		SetWidgetLine(WeakWidget.Pin(), StatusLine);
	}
}
