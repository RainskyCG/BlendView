// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/SBlendViewViewportToolbarControls.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

namespace
{

	constexpr float ToolbarButtonHeight = 24.0f;
	constexpr float ToolbarShadowHeight = 1.0f;
	constexpr float ToolbarControlHeight = ToolbarButtonHeight + ToolbarShadowHeight;
	constexpr float PivotControlWidth = 44.0f;
	constexpr float SnapControlHeight = ToolbarButtonHeight;
	constexpr float SnapLeftSegmentWidth = SnapControlHeight;
	constexpr float SnapMinRightSegmentWidth = 58.0f;
	constexpr float SnapCornerRadius = 4.0f;
	constexpr float SnapLayerInset = 1.0f;
	constexpr float SnapLayerGap = 1.0f;
	constexpr float SnapInnerRadius = 3.0f;
	const FLinearColor SnapBackgroundColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("0F0F0F")));
	const FLinearColor SnapMagnetBackgroundColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("545454")));
	const FLinearColor SnapShellColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("343434")));
	const FLinearColor SnapActiveColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4772B3")));
	const FLinearColor SnapContentColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C8C8C8")));
	const FLinearColor SnapHoverContentColor = FLinearColor::White;
	const FLinearColor ToolbarShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.45f);
	const FSlateRoundedBoxBrush SnapShellBrush(
		FLinearColor::White,
		SnapCornerRadius);
	const FSlateRoundedBoxBrush SnapLeftSegmentBrush(
		FLinearColor::White,
		FVector4(SnapInnerRadius, 0.0f, 0.0f, SnapInnerRadius));
	const FSlateRoundedBoxBrush SnapRightSegmentBrush(
		FLinearColor::White,
		FVector4(0.0f, SnapInnerRadius, SnapInnerRadius, 0.0f));

	FLinearColor GetRaisedColor(const FLinearColor& BaseColor, const float Amount = 0.12f)
	{
		return FLinearColor(
			FMath::Min(BaseColor.R + Amount, 1.0f),
			FMath::Min(BaseColor.G + Amount, 1.0f),
			FMath::Min(BaseColor.B + Amount, 1.0f),
			BaseColor.A);
	}

	void DrawToolbarRoundedShadow(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FVector2D& Size,
		const ESlateDrawEffect DrawEffects)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				Size,
				FSlateLayoutTransform(FVector2D(0.0f, ToolbarShadowHeight))),
			&SnapShellBrush,
			DrawEffects,
			ToolbarShadowColor);
	}


}

class SBlendViewPivotMenuButton final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendViewPivotMenuButton)
		{}
		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)
		SLATE_ATTRIBUTE(FText, ToolTip)
		SLATE_EVENT(FOnGetContent, OnGetMenuContent)
		SLATE_EVENT(FOnIsOpenChanged, OnMenuOpenChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Icon = InArgs._Icon;
		SetToolTipText(InArgs._ToolTip);

		ChildSlot
		[
			SAssignNew(MenuAnchor, SMenuAnchor)
			.Placement(MenuPlacement_ComboBox)
			.Method(EPopupMethod::UseCurrentWindow)
			.UseApplicationMenuStack(true)
			.OnGetMenuContent(InArgs._OnGetMenuContent)
			.OnMenuOpenChanged(InArgs._OnMenuOpenChanged)
			[
				SNew(SBox)
				.WidthOverride(PivotControlWidth)
				.HeightOverride(ToolbarControlHeight)
			]
		];
	}

	bool IsMenuOpen() const
	{
		return MenuAnchor.IsValid() && MenuAnchor->IsOpen();
	}

	void SetMenuOpen(const bool bIsOpen)
	{
		if (MenuAnchor.IsValid())
		{
			MenuAnchor->SetIsOpen(bIsOpen, false);
		}
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D(PivotControlWidth, ToolbarControlHeight);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return HandlePointerClick(MouseEvent);
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return HandlePointerClick(MouseEvent);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const bool bParentEnabled) const override
	{
		const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled)
			? ESlateDrawEffect::None
			: ESlateDrawEffect::DisabledEffect;
		const FVector2D Size(AllottedGeometry.GetLocalSize().X, ToolbarButtonHeight);
		constexpr float PivotLayerInset = 1.0f;
		const FVector2D InnerOffset(PivotLayerInset, PivotLayerInset);
		const FVector2D InnerSize(
			FMath::Max(1.0f, Size.X - PivotLayerInset * 2.0f),
			FMath::Max(1.0f, Size.Y - PivotLayerInset * 2.0f));
		const bool bHovered = IsHovered();
		const bool bMenuOpen = IsMenuOpen();
		const FLinearColor ForegroundColor = bHovered ? SnapHoverContentColor : SnapContentColor;

		DrawToolbarRoundedShadow(
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			Size,
			DrawEffects);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D::ZeroVector)),
			&SnapShellBrush,
			DrawEffects,
			SnapShellColor);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(InnerSize, FSlateLayoutTransform(InnerOffset)),
			&SnapShellBrush,
			DrawEffects,
			bMenuOpen ? SnapActiveColor : (bHovered ? GetRaisedColor(SnapBackgroundColor) : SnapBackgroundColor));

		if (const FSlateBrush* IconBrush = Icon.Get(nullptr))
		{
			const FVector2D IconSize(16.0f, 16.0f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 3,
				AllottedGeometry.ToPaintGeometry(
				IconSize,
				FSlateLayoutTransform(FVector2D(
						7.0f,
						FMath::RoundToFloat((Size.Y - IconSize.Y) * 0.5f)))),
				IconBrush,
				DrawEffects,
				bMenuOpen ? FLinearColor::White : ForegroundColor);
		}

		if (const FSlateBrush* ArrowBrush = FAppStyle::GetBrush(TEXT("Icons.ChevronDown")))
		{
			const FVector2D ArrowSize(9.0f, 9.0f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 3,
				AllottedGeometry.ToPaintGeometry(
					ArrowSize,
					FSlateLayoutTransform(FVector2D(
						Size.X - ArrowSize.X - 6.0f,
						FMath::RoundToFloat((Size.Y - ArrowSize.Y) * 0.5f)))),
				ArrowBrush,
				DrawEffects,
				bMenuOpen ? FLinearColor::White : ForegroundColor);
		}

		return SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 4,
			InWidgetStyle,
			bParentEnabled);
	}

private:
	FReply HandlePointerClick(const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		if (MenuAnchor.IsValid())
		{
			const bool bOpenMenu = !MenuAnchor->IsOpen();
			MenuAnchor->SetIsOpen(bOpenMenu, bOpenMenu);
		}
		return FReply::Handled();
	}

	TSharedPtr<SMenuAnchor> MenuAnchor;
	TAttribute<const FSlateBrush*> Icon;
};

class SBlendViewSnapSplitButton final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendViewSnapSplitButton)
		{}
		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)
		SLATE_ATTRIBUTE(FText, Label)
		SLATE_ATTRIBUTE(FText, ToolTip)
		SLATE_EVENT(FOnGetContent, OnGetMenuContent)
		SLATE_EVENT(FOnIsOpenChanged, OnMenuOpenChanged)
		SLATE_EVENT(FSimpleDelegate, OnTogglePersistentSnap)
		SLATE_EVENT(FBlendViewIsPersistentSnapEnabled, IsPersistentSnapEnabled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Icon = InArgs._Icon;
		Label = InArgs._Label;
		OnTogglePersistentSnap = InArgs._OnTogglePersistentSnap;
		IsPersistentSnapEnabled = InArgs._IsPersistentSnapEnabled;

		SetToolTipText(InArgs._ToolTip);

		ChildSlot
		[
			SAssignNew(MenuAnchor, SMenuAnchor)
			.Placement(MenuPlacement_ComboBox)
			.Method(EPopupMethod::UseCurrentWindow)
			.UseApplicationMenuStack(true)
			.OnGetMenuContent(InArgs._OnGetMenuContent)
			.OnMenuOpenChanged(InArgs._OnMenuOpenChanged)
			[
				SNew(SBox)
				.WidthOverride(TAttribute<FOptionalSize>::Create(TAttribute<FOptionalSize>::FGetter::CreateSP(
					this,
					&SBlendViewSnapSplitButton::GetDesiredWidthOptional)))
				.HeightOverride(ToolbarControlHeight)
			]
		];
	}

	bool IsMenuOpen() const
	{
		return MenuAnchor.IsValid() && MenuAnchor->IsOpen();
	}

	void SetMenuOpen(const bool bIsOpen)
	{
		if (MenuAnchor.IsValid())
		{
			MenuAnchor->SetIsOpen(bIsOpen, false);
		}
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D(GetDesiredWidth(), ToolbarControlHeight);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return HandlePointerClick(MyGeometry, MouseEvent);
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return HandlePointerClick(MyGeometry, MouseEvent);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const bool bParentEnabled) const override
	{
		const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled)
			? ESlateDrawEffect::None
			: ESlateDrawEffect::DisabledEffect;
		const FVector2D Size(AllottedGeometry.GetLocalSize().X, SnapControlHeight);
		const float LocalMouseX = AllottedGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos()).X;
		const bool bHoveringLeft = IsHovered() && LocalMouseX <= SnapLeftSegmentWidth;
		const bool bHoveringRight = IsHovered() && LocalMouseX > SnapLeftSegmentWidth;
		const FLinearColor IconColor = bHoveringLeft ? SnapHoverContentColor : SnapContentColor;
		const bool bMenuOpen = IsMenuOpen();
		const FLinearColor TextColor = (bMenuOpen || bHoveringRight) ? SnapHoverContentColor : SnapContentColor;
		const bool bPersistentSnapEnabled =
			IsPersistentSnapEnabled.IsBound() && IsPersistentSnapEnabled.Execute();
		const FVector2D SegmentSize(
			SnapLeftSegmentWidth - SnapLayerInset - SnapLayerGap,
			Size.Y - SnapLayerInset * 2.0f);
		const FVector2D LeftSegmentOffset(SnapLayerInset, SnapLayerInset);
		const FVector2D RightSegmentOffset(SnapLeftSegmentWidth, SnapLayerInset);
		const FVector2D RightSegmentSize(
			FMath::Max(1.0f, Size.X - SnapLeftSegmentWidth - SnapLayerInset),
			Size.Y - SnapLayerInset * 2.0f);

		DrawToolbarRoundedShadow(
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			Size,
			DrawEffects);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D::ZeroVector)),
			&SnapShellBrush,
			DrawEffects,
			SnapShellColor);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(SegmentSize, FSlateLayoutTransform(LeftSegmentOffset)),
			&SnapLeftSegmentBrush,
			DrawEffects,
			bPersistentSnapEnabled
				? SnapActiveColor
				: (bHoveringLeft ? GetRaisedColor(SnapMagnetBackgroundColor) : SnapMagnetBackgroundColor));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(RightSegmentSize, FSlateLayoutTransform(RightSegmentOffset)),
			&SnapRightSegmentBrush,
			DrawEffects,
			bMenuOpen ? SnapActiveColor : (bHoveringRight ? GetRaisedColor(SnapBackgroundColor) : SnapBackgroundColor));

		if (const FSlateBrush* IconBrush = Icon.Get(nullptr))
		{
			const FVector2D IconSize(16.0f, 16.0f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 3,
				AllottedGeometry.ToPaintGeometry(
					IconSize,
					FSlateLayoutTransform(FVector2D(
						FMath::RoundToFloat((SnapLeftSegmentWidth - IconSize.X) * 0.5f),
						FMath::RoundToFloat((Size.Y - IconSize.Y) * 0.5f)))),
				IconBrush,
				DrawEffects,
				bPersistentSnapEnabled ? FLinearColor::White : IconColor);
		}

		const FText LabelText = Label.Get(FText::GetEmpty());
		const FSlateFontInfo FontInfo = FAppStyle::GetFontStyle(TEXT("NormalFont"));
		const FVector2D TextSize = MeasureText(LabelText, FontInfo);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 3,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(FMath::Min(TextSize.X, Size.X - SnapLeftSegmentWidth - 24.0f), TextSize.Y),
				FSlateLayoutTransform(FVector2D(
					SnapLeftSegmentWidth + 8.0f,
					FMath::RoundToFloat((Size.Y - TextSize.Y) * 0.5f)))),
			LabelText,
			FontInfo,
			DrawEffects,
			TextColor);

		if (const FSlateBrush* ArrowBrush = FAppStyle::GetBrush(TEXT("Icons.ChevronDown")))
		{
			const FVector2D ArrowSize(9.0f, 9.0f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 3,
				AllottedGeometry.ToPaintGeometry(
					ArrowSize,
					FSlateLayoutTransform(FVector2D(
						Size.X - ArrowSize.X - 6.0f,
						FMath::RoundToFloat((Size.Y - ArrowSize.Y) * 0.5f)))),
				ArrowBrush,
				DrawEffects,
				TextColor);
		}

		return SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 5,
			InWidgetStyle,
			bParentEnabled);
	}

private:
	FReply HandlePointerClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		if (LocalPosition.X <= SnapLeftSegmentWidth)
		{
			if (OnTogglePersistentSnap.IsBound())
			{
				OnTogglePersistentSnap.Execute();
			}
			return FReply::Handled();
		}

		if (MenuAnchor.IsValid())
		{
			const bool bOpenMenu = !MenuAnchor->IsOpen();
			MenuAnchor->SetIsOpen(bOpenMenu, bOpenMenu);
		}
		return FReply::Handled();
	}

	float GetDesiredWidth() const
	{
		const FSlateFontInfo FontInfo = FAppStyle::GetFontStyle(TEXT("NormalFont"));
		const float TextWidth = MeasureText(Label.Get(FText::GetEmpty()), FontInfo).X;
		return SnapLeftSegmentWidth + FMath::Max(SnapMinRightSegmentWidth, TextWidth + 28.0f);
	}

	FOptionalSize GetDesiredWidthOptional() const
	{
		return FOptionalSize(GetDesiredWidth());
	}

	FVector2D MeasureText(const FText& Text, const FSlateFontInfo& FontInfo) const
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D::ZeroVector;
		}

		const TSharedRef<FSlateFontMeasure> FontMeasureService =
			FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FontMeasureService->Measure(Text, FontInfo);
	}

	TSharedPtr<SMenuAnchor> MenuAnchor;
	TAttribute<const FSlateBrush*> Icon;
	TAttribute<FText> Label;
	FSimpleDelegate OnTogglePersistentSnap;
	FBlendViewIsPersistentSnapEnabled IsPersistentSnapEnabled;
};

void SBlendViewViewportToolbarControls::Construct(const FArguments& InArgs)
{
	OnGetPivotMenuContent = InArgs._OnGetPivotMenuContent;
	OnGetSnapMenuContent = InArgs._OnGetSnapMenuContent;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
		[
			SAssignNew(PivotMenuButton, SBlendViewPivotMenuButton)
			.Icon(InArgs._PivotIcon)
			.ToolTip(InArgs._PivotToolTip)
			.OnGetMenuContent(this, &SBlendViewViewportToolbarControls::HandleGetPivotMenuContent)
			.OnMenuOpenChanged(this, &SBlendViewViewportToolbarControls::HandleMenuOpenChanged, EMenuKind::Pivot)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
		[
			SAssignNew(SnapSplitButton, SBlendViewSnapSplitButton)
			.Icon(InArgs._SnapIcon)
			.Label(InArgs._SnapTargetLabel)
			.ToolTip(InArgs._SnapToolTip)
			.OnGetMenuContent(this, &SBlendViewViewportToolbarControls::HandleGetSnapMenuContent)
			.OnMenuOpenChanged(this, &SBlendViewViewportToolbarControls::HandleMenuOpenChanged, EMenuKind::Snap)
			.OnTogglePersistentSnap(InArgs._OnTogglePersistentSnap)
			.IsPersistentSnapEnabled(InArgs._IsPersistentSnapEnabled)
		]
	];
}

SBlendViewViewportToolbarControls::~SBlendViewViewportToolbarControls() = default;

TSharedRef<SWidget> SBlendViewViewportToolbarControls::HandleGetPivotMenuContent()
{
	PivotMenuContent = OnGetPivotMenuContent.IsBound()
		? OnGetPivotMenuContent.Execute()
		: SNullWidget::NullWidget;
	return PivotMenuContent.ToSharedRef();
}

TSharedRef<SWidget> SBlendViewViewportToolbarControls::HandleGetSnapMenuContent()
{
	SnapMenuContent = OnGetSnapMenuContent.IsBound()
		? OnGetSnapMenuContent.Execute()
		: SNullWidget::NullWidget;
	return SnapMenuContent.ToSharedRef();
}

void SBlendViewViewportToolbarControls::HandleMenuOpenChanged(
	const bool bIsOpen,
	const EMenuKind MenuKind)
{
	if (!bIsOpen)
	{
		if (MenuKind == EMenuKind::Pivot)
		{
			PivotMenuContent.Reset();
		}
		else
		{
			SnapMenuContent.Reset();
		}
		return;
	}

	// Let Slate's application menu stack handle outside-click dismissal. BlendView only
	// keeps the two centered toolbar drop-downs mutually exclusive.
	if (MenuKind == EMenuKind::Pivot)
	{
		CloseMenu(EMenuKind::Snap);
	}
	else
	{
		CloseMenu(EMenuKind::Pivot);
	}
}

void SBlendViewViewportToolbarControls::CloseMenu(const EMenuKind MenuKind)
{
	if (MenuKind == EMenuKind::Pivot && PivotMenuButton.IsValid() && PivotMenuButton->IsMenuOpen())
	{
		PivotMenuButton->SetMenuOpen(false);
	}
	else if (MenuKind == EMenuKind::Snap && SnapSplitButton.IsValid() && SnapSplitButton->IsMenuOpen())
	{
		SnapSplitButton->SetMenuOpen(false);
	}
}
