// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/SBlendViewPieMenu.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Editor.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Localization/BlendViewLocalization.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"

namespace
{
	// Layout
	constexpr float PieMenuWidth = 620.0f;
	constexpr float PieRingCenterY = 176.0f;
	constexpr float PieMenuElementScale = 1.2f;
	constexpr float TooltipDelaySeconds = 1.0f;
	constexpr float ButtonRadius = 3.0f;
	constexpr float ButtonHeight = 43.0f;
	constexpr float ButtonInnerInset = 1.0f;
	constexpr float PopupAnimationSeconds = 0.12f;
	constexpr float IconColumnWidth = 41.0f;
	constexpr float ButtonRightPadding = 11.0f;
	constexpr float ButtonHintPadding = 20.0f;
	constexpr float TopButtonMinWidth = 118.0f;
	constexpr float BottomButtonMinWidth = 122.0f;
	constexpr float BottomShellX = 190.0f;
	constexpr float BottomShellY = 292.0f;
	constexpr float BottomCellWidth = 134.0f;
	constexpr float BottomCellHeight = 35.0f;
	constexpr float BottomCellGap = 2.0f;

	// Colors
	const FLinearColor ButtonShellColor(0.0f, 0.0f, 0.0f, 0.78f);
	const FLinearColor TopButtonInnerColor = FLinearColor::FromSRGBColor(FColor(0x17, 0x17, 0x17, 0xE6));
	const FLinearColor TopButtonInnerHoverColor = FLinearColor::FromSRGBColor(FColor(0x53, 0x53, 0x53, 0xEE));
	const FLinearColor BottomButtonInnerColor = FLinearColor::FromSRGBColor(FColor(0x53, 0x53, 0x53, 0xE6));
	const FLinearColor BottomButtonInnerHoverColor = FLinearColor::FromSRGBColor(FColor(0x64, 0x64, 0x64, 0xEE));
	const FLinearColor ButtonDisabledColor(0.18f, 0.18f, 0.18f, 0.54f);
	const FLinearColor PieTextColor(0.84f, 0.84f, 0.84f, 1.0f);
	const FLinearColor PieMutedTextColor(0.55f, 0.55f, 0.55f, 0.95f);
	const FLinearColor PieDisabledTextColor(0.46f, 0.46f, 0.46f, 0.62f);
	const FLinearColor PieTextShadowColor(0.0f, 0.0f, 0.0f, 0.65f);
	const FLinearColor PieRingColor = FLinearColor::FromSRGBColor(FColor(0x17, 0x17, 0x17, 0xFF));
	const FLinearColor PieRingHighlightColor = FLinearColor::FromSRGBColor(FColor(0x53, 0x53, 0x53, 0xFF));
	const FLinearColor TooltipColor = FLinearColor::FromSRGBColor(FColor(0x17, 0x17, 0x17, 0xF4));
	const FLinearColor TooltipShadowColor(0.0f, 0.0f, 0.0f, 0.22f);

	// Brushes and style names
	const FSlateRoundedBoxBrush ShellBrush(FLinearColor::White, ButtonRadius);
	const FSlateRoundedBoxBrush InnerBrush(FLinearColor::White, ButtonRadius - 1.0f);
	const FSlateRoundedBoxBrush TooltipBrush(FLinearColor::White, 2.0f);
	const FSlateRoundedBoxBrush BottomTopLeftBrush(FLinearColor::White, FVector4(ButtonRadius - 1.0f, 0.0f, 0.0f, 0.0f));
	const FSlateRoundedBoxBrush BottomTopRightBrush(FLinearColor::White, FVector4(0.0f, ButtonRadius - 1.0f, 0.0f, 0.0f));
	const FSlateRoundedBoxBrush BottomLowerLeftBrush(FLinearColor::White, FVector4(0.0f, 0.0f, 0.0f, ButtonRadius - 1.0f));
	const FSlateRoundedBoxBrush BottomLowerRightBrush(FLinearColor::White, FVector4(0.0f, 0.0f, ButtonRadius - 1.0f, 0.0f));
	const FName PieBlendViewStyleSetName(TEXT("BlendViewStyle"));
	const FName PiePivotBoundingBoxCenterIconName(TEXT("BlendView.PivotBoundingBoxCenter"));
	const FName PiePivotCursorIconName(TEXT("BlendView.PivotCursor"));
	const FName PiePivotActiveItemIconName(TEXT("BlendView.PivotActiveItem"));

	FVector2D MeasureText(const FText& Text, const FSlateFontInfo& FontInfo)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D::ZeroVector;
		}

		const TSharedRef<FSlateFontMeasure> FontMeasureService =
			FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FontMeasureService->Measure(Text, FontInfo);
	}

	FSlateFontInfo ScaledFont(const FName StyleName, const float Scale = PieMenuElementScale)
	{
		FSlateFontInfo FontInfo = FAppStyle::GetFontStyle(StyleName);
		FontInfo.Size = FMath::Max(1, FMath::RoundToInt(static_cast<float>(FontInfo.Size) * Scale));
		return FontInfo;
	}

	FVector2D ScaleMenuPoint(const FVector2D& Point)
	{
		const FVector2D Center(PieMenuWidth * 0.5f, PieRingCenterY);
		return Center + (Point - Center) * PieMenuElementScale;
	}

	void DrawText(
		const FGeometry& Geometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FVector2D& Position,
		const FVector2D& Size,
		const FText& Text,
		const FSlateFontInfo& Font,
		const ESlateDrawEffect DrawEffects,
		const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
			Text,
			Font,
			DrawEffects,
			Color);
	}

	void DrawShadowedText(
		const FGeometry& Geometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FVector2D& Position,
		const FVector2D& Size,
		const FText& Text,
		const FSlateFontInfo& Font,
		const ESlateDrawEffect DrawEffects,
		const FLinearColor& Color)
	{
		DrawText(
			Geometry,
			OutDrawElements,
			LayerId,
			Position + FVector2D(1.0f, 1.0f),
			Size,
			Text,
			Font,
			DrawEffects,
			PieTextShadowColor);
		DrawText(
			Geometry,
			OutDrawElements,
			LayerId + 1,
			Position,
			Size,
			Text,
			Font,
			DrawEffects,
			Color);
	}

	void DrawLine(
		const FGeometry& Geometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FVector2D& Start,
		const FVector2D& End,
		const ESlateDrawEffect DrawEffects,
		const FLinearColor& Color,
		const float Thickness)
	{
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			{Start, End},
			DrawEffects,
			Color,
			true,
			Thickness);
	}

	void DrawBox(
		const FGeometry& Geometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FVector2D& Position,
		const FVector2D& Size,
		const FSlateBrush* Brush,
		const ESlateDrawEffect DrawEffects,
		const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
			Brush,
			DrawEffects,
			Color);
	}

	void DrawLayeredButton(
		const FGeometry& Geometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FVector2D& Position,
		const FVector2D& Size,
		const ESlateDrawEffect DrawEffects,
		const FLinearColor& InnerColor)
	{
		DrawBox(
			Geometry,
			OutDrawElements,
			LayerId,
			Position,
			Size,
			&ShellBrush,
			DrawEffects,
			ButtonShellColor);
		DrawBox(
			Geometry,
			OutDrawElements,
			LayerId + 1,
			Position + FVector2D(ButtonInnerInset),
			Size - FVector2D(ButtonInnerInset * 2.0f),
			&InnerBrush,
			DrawEffects,
			InnerColor);
	}

	const FSlateRoundedBoxBrush* GetBottomInnerBrush(const int32 EntryIndex)
	{
		switch (EntryIndex)
		{
		case 4:
			return &BottomTopLeftBrush;
		case 5:
			return &BottomTopRightBrush;
		case 6:
			return &BottomLowerLeftBrush;
		case 7:
			return &BottomLowerRightBrush;
		default:
			return &InnerBrush;
		}
	}

	bool IsActiveTargetEnabled()
	{
		return FBlendViewCursorOriginActions::CanExecute(EBlendViewCursorOriginAction::OriginToActive);
	}

	void DrawRingSegment(
		const FGeometry& Geometry,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FVector2D& Center,
		const float InnerRadius,
		const float OuterRadius,
		const float FeatherSize,
		const float StartAngle,
		const float EndAngle,
		const int32 SegmentCount,
		const ESlateDrawEffect DrawEffects,
		const FLinearColor& Color)
	{
		if (!FSlateApplication::IsInitialized() || SegmentCount <= 0 || OuterRadius <= InnerRadius || FeatherSize <= 0.0f)
		{
			return;
		}

		const FSlateBrush* WhiteBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
		if (!WhiteBrush)
		{
			return;
		}

		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		Vertices.Reserve((SegmentCount + 1) * 4);
		Indices.Reserve(SegmentCount * 18);

		FLinearColor TransparentColor = Color;
		TransparentColor.A = 0.0f;
		const FColor SolidVertexColor = Color.ToFColor(true);
		const FColor TransparentVertexColor = TransparentColor.ToFColor(true);
		const FSlateRenderTransform RenderTransform = Geometry.GetAccumulatedRenderTransform();
		for (int32 Index = 0; Index <= SegmentCount; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(SegmentCount);
			const float Angle = FMath::Lerp(StartAngle, EndAngle, Alpha);
			const FVector2f Direction(FMath::Cos(Angle), FMath::Sin(Angle));
			const float Radii[4] = {
				OuterRadius + FeatherSize,
				OuterRadius,
				InnerRadius,
				InnerRadius - FeatherSize
			};
			const FColor Colors[4] = {
				TransparentVertexColor,
				SolidVertexColor,
				SolidVertexColor,
				TransparentVertexColor
			};

			for (int32 RingIndex = 0; RingIndex < 4; ++RingIndex)
			{
				const FVector2f Position(
					Center.X + Direction.X * Radii[RingIndex],
					Center.Y + Direction.Y * Radii[RingIndex]);
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
					RenderTransform,
					Position,
					FVector2f(0.0f, 0.0f),
					Colors[RingIndex]));
			}
		}

		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			const SlateIndex Base0 = static_cast<SlateIndex>(Index * 4);
			const SlateIndex Base1 = static_cast<SlateIndex>((Index + 1) * 4);
			for (int32 BandIndex = 0; BandIndex < 3; ++BandIndex)
			{
				const SlateIndex A = static_cast<SlateIndex>(Base0 + BandIndex);
				const SlateIndex B = static_cast<SlateIndex>(Base0 + BandIndex + 1);
				const SlateIndex C = static_cast<SlateIndex>(Base1 + BandIndex);
				const SlateIndex D = static_cast<SlateIndex>(Base1 + BandIndex + 1);
				Indices.Add(A);
				Indices.Add(C);
				Indices.Add(B);
				Indices.Add(C);
				Indices.Add(D);
				Indices.Add(B);
			}
		}

		const FSlateResourceHandle ResourceHandle =
			FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
		FSlateDrawElement::MakeCustomVerts(
			OutDrawElements,
			LayerId,
			ResourceHandle,
			Vertices,
			Indices,
			nullptr,
			0,
			0,
			DrawEffects);
	}

	const FSlateBrush* GetBlendViewBrush(const FName BrushName)
	{
		const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle(PieBlendViewStyleSetName);
		return Style ? Style->GetBrush(BrushName) : nullptr;
	}

	const FSlateBrush* GetEntryIconBrush(const int32 EntryIndex)
	{
		switch (EntryIndex)
		{
		case 0:
		case 1:
			return GetBlendViewBrush(PiePivotCursorIconName);
		case 2:
		case 3:
			return FAppStyle::GetBrush(TEXT("LevelEditor.SelectMode"));
		case 4:
			return GetBlendViewBrush(PiePivotBoundingBoxCenterIconName);
		case 5:
			return GetBlendViewBrush(PiePivotCursorIconName);
		case 6:
			return GetBlendViewBrush(PiePivotActiveItemIconName);
		case 7:
			return FAppStyle::GetBrush(TEXT("GraphEditor.AlignNodesBottom"));
		default:
			return nullptr;
		}
	}
}

void SBlendViewPieMenu::Construct(const FArguments& InArgs)
{
	OnRequestClose = InArgs._OnRequestClose;
	OnAction = InArgs._OnAction;

	Entries = {
		{FBlendViewLocalization::Text(TEXT("\u5230\u539F\u70B9"), TEXT("to Origin")), FBlendViewLocalization::Text(TEXT("\u5C06\u6E38\u6807\u79FB\u52A8\u5230\u4E16\u754C\u539F\u70B9\u3002"), TEXT("Move the 3D cursor to the world origin.")), FText::FromString(TEXT("7")), FVector2D(142.0f, 60.0f), FVector2D(106.0f, ButtonHeight), EBlendViewCursorOriginAction::CursorToOrigin, true},
		{FBlendViewLocalization::Text(TEXT("\u5230\u6240\u9009"), TEXT("to Selected")), FBlendViewLocalization::Text(TEXT("\u5C06\u6E38\u6807\u79FB\u52A8\u5230\u5F53\u524D\u9009\u62E9\u4E2D\u5FC3\u3002"), TEXT("Move the 3D cursor to the current selection center.")), FText::FromString(TEXT("4")), FVector2D(74.0f, 130.0f), FVector2D(124.0f, ButtonHeight), EBlendViewCursorOriginAction::CursorToSelected, true},
		{FBlendViewLocalization::Text(TEXT("\u5230\u6E38\u6807\uFF0C\u504F\u79FB"), TEXT("to Cursor, Offset")), FBlendViewLocalization::Text(TEXT("\u5C06\u6240\u9009\u5185\u5BB9\u79FB\u52A8\u5230\u6E38\u6807\uFF0C\u5E76\u4FDD\u6301\u76F8\u5BF9\u504F\u79FB\u3002"), TEXT("Move the selection to the cursor while keeping relative offsets.")), FText::FromString(TEXT("9")), FVector2D(372.0f, 60.0f), FVector2D(132.0f, ButtonHeight), EBlendViewCursorOriginAction::SelectionToCursorOffset, true},
		{FBlendViewLocalization::Text(TEXT("\u5230\u6E38\u6807"), TEXT("to Cursor")), FBlendViewLocalization::Text(TEXT("\u5C06\u6240\u9009\u5185\u5BB9\u79FB\u52A8\u5230\u6E38\u6807\u4F4D\u7F6E\u3002"), TEXT("Move the selection to the cursor position.")), FText::FromString(TEXT("6")), FVector2D(422.0f, 130.0f), FVector2D(106.0f, ButtonHeight), EBlendViewCursorOriginAction::SelectionToCursor, true},
		{FBlendViewLocalization::Text(TEXT("\u5230\u51E0\u4F55\u4E2D\u5FC3"), TEXT("to Geometry")), FBlendViewLocalization::Text(TEXT("\u5C06\u7269\u4F53\u539F\u70B9\u79FB\u52A8\u5230\u51E0\u4F55\u4E2D\u5FC3\u3002"), TEXT("Move the object origin to the geometry center.")), FText::GetEmpty(), FVector2D(BottomShellX + 1.0f, BottomShellY + 1.0f), FVector2D(BottomCellWidth, BottomCellHeight), EBlendViewCursorOriginAction::OriginToGeometry, true},
		{FBlendViewLocalization::Text(TEXT("\u5230\u6E38\u6807"), TEXT("to Cursor")), FBlendViewLocalization::Text(TEXT("\u5C06\u7269\u4F53\u539F\u70B9\u79FB\u52A8\u5230\u6E38\u6807\u3002"), TEXT("Move the object origin to the 3D cursor.")), FText::GetEmpty(), FVector2D(BottomShellX + 1.0f + BottomCellWidth + BottomCellGap, BottomShellY + 1.0f), FVector2D(BottomCellWidth, BottomCellHeight), EBlendViewCursorOriginAction::OriginToCursor, true},
		{FBlendViewLocalization::Text(TEXT("\u5230\u6D3B\u52A8"), TEXT("to Active")), FBlendViewLocalization::Text(TEXT("\u5C06\u7269\u4F53\u539F\u70B9\u79FB\u52A8\u5230\u6D3B\u52A8\u9879\u3002"), TEXT("Move the object origin to the active element.")), FText::GetEmpty(), FVector2D(BottomShellX + 1.0f, BottomShellY + 1.0f + BottomCellHeight + BottomCellGap), FVector2D(BottomCellWidth, BottomCellHeight), EBlendViewCursorOriginAction::OriginToActive, false},
		{FBlendViewLocalization::Text(TEXT("\u5230\u5E95\u90E8"), TEXT("to Bottom")), FBlendViewLocalization::Text(TEXT("\u5C06\u7269\u4F53\u539F\u70B9\u79FB\u52A8\u5230\u5E95\u90E8\u3002"), TEXT("Move the object origin to the bottom center.")), FText::GetEmpty(), FVector2D(BottomShellX + 1.0f + BottomCellWidth + BottomCellGap, BottomShellY + 1.0f + BottomCellHeight + BottomCellGap), FVector2D(BottomCellWidth, BottomCellHeight), EBlendViewCursorOriginAction::OriginToBottom, true}
	};

	OpenTimeSeconds = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetCurrentTime()
		: 0.0;
}

void SBlendViewPieMenu::Tick(
	const FGeometry& AllottedGeometry,
	const double InCurrentTime,
	const float InDeltaTime)
{
	SLeafWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (InCurrentTime - OpenTimeSeconds < PopupAnimationSeconds)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	if (HoveredEntry != INDEX_NONE && InCurrentTime - HoverStartTimeSeconds < TooltipDelaySeconds)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SBlendViewPieMenu::SetAnchorViewportPosition(
	const FVector2D& InAnchorViewportPosition,
	const FIntPoint& InViewportSize)
{
	AnchorViewportPosition = InAnchorViewportPosition;
	PointerViewportPosition = InAnchorViewportPosition;
	ViewportSize = InViewportSize;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlendViewPieMenu::SetPointerViewportPosition(const FVector2D& InPointerViewportPosition)
{
	PointerViewportPosition = InPointerViewportPosition;
	if (bHasLastPaintGeometry)
	{
		UpdateHoveredEntryFromLocalPosition(ViewportToLocal(PointerViewportPosition));
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool SBlendViewPieMenu::TryGetActionAtViewportPosition(
	const FVector2D& ViewportPosition,
	EBlendViewCursorOriginAction& OutAction) const
{
	if (!bHasLastPaintGeometry)
	{
		return false;
	}

	const FVector2D Center = ViewportToLocal(AnchorViewportPosition);
	const FVector2D MenuOrigin = Center - FVector2D(PieMenuWidth * 0.5f, PieRingCenterY);
	const FVector2D LocalPosition = ViewportToLocal(ViewportPosition);
	const int32 EntryIndex = FindEntryAt(LocalPosition - MenuOrigin);
	if (!IsEntryEnabled(EntryIndex))
	{
		return false;
	}

	OutAction = Entries[EntryIndex].Action;
	return true;
}

bool SBlendViewPieMenu::TryGetHoveredAction(EBlendViewCursorOriginAction& OutAction) const
{
	if (!Entries.IsValidIndex(HoveredEntry))
	{
		return false;
	}

	if (!IsEntryEnabled(HoveredEntry))
	{
		return false;
	}

	OutAction = Entries[HoveredEntry].Action;
	return true;
}

FVector2D SBlendViewPieMenu::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D::ZeroVector;
}

FReply SBlendViewPieMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}

	const FVector2D Center = ViewportToLocal(AnchorViewportPosition);
	const FVector2D MenuOrigin = Center - FVector2D(PieMenuWidth * 0.5f, PieRingCenterY);
	const int32 EntryIndex = FindEntryAt(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()) - MenuOrigin);
	if (EntryIndex != INDEX_NONE && IsEntryEnabled(EntryIndex))
	{
		if (OnAction.IsBound())
		{
			OnAction.Execute(Entries[EntryIndex].Action);
		}
		if (OnRequestClose.IsBound())
		{
			OnRequestClose.Execute();
		}
		return FReply::Handled();
	}

	return FReply::Handled();
}

FReply SBlendViewPieMenu::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// The input processor owns viewport-coordinate pointer updates. Keep the widget-level
	// move path only for hover feedback if Slate sends mouse events directly to this overlay.
	UpdateHoveredEntryFromLocalPosition(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

void SBlendViewPieMenu::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SLeafWidget::OnMouseLeave(MouseEvent);
	HoveredEntry = INDEX_NONE;
	HoverStartTimeSeconds = 0.0;
}

int32 SBlendViewPieMenu::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	LastPaintGeometry = AllottedGeometry;
	bHasLastPaintGeometry = true;

	const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled)
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;
	const FVector2D Center = ViewportToLocal(AnchorViewportPosition);
	const FVector2D PointerLocalPosition = ViewportToLocal(PointerViewportPosition);
	const FVector2D MenuOrigin = Center - FVector2D(PieMenuWidth * 0.5f, PieRingCenterY);
	const int32 CurrentHoveredEntry = FindEntryAt(PointerLocalPosition - MenuOrigin);

	PaintCenter(AllottedGeometry, OutDrawElements, LayerId, DrawEffects, MenuOrigin, Center);

	DrawBox(
		AllottedGeometry,
		OutDrawElements,
		LayerId + 3,
		MenuOrigin + FVector2D(ComputeBottomShellRect().Left, ComputeBottomShellRect().Top),
		FVector2D(ComputeBottomShellRect().GetSize().X, ComputeBottomShellRect().GetSize().Y),
		&ShellBrush,
		DrawEffects,
		ButtonShellColor);

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		PaintEntry(AllottedGeometry, OutDrawElements, LayerId + 4, DrawEffects, EntryIndex, CurrentHoveredEntry);
	}

	PaintTooltip(AllottedGeometry, OutDrawElements, LayerId + 9, DrawEffects, CurrentHoveredEntry, MenuOrigin);

	return LayerId + 14;
}

int32 SBlendViewPieMenu::FindEntryAt(const FVector2D& LocalPosition) const
{
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FSlateRect EntryRect = ComputeEntryRect(EntryIndex);
		if (LocalPosition.X >= EntryRect.Left &&
			LocalPosition.X <= EntryRect.Right &&
			LocalPosition.Y >= EntryRect.Top &&
			LocalPosition.Y <= EntryRect.Bottom)
		{
			return EntryIndex;
		}
	}
	return INDEX_NONE;
}

bool SBlendViewPieMenu::IsEntryEnabled(const int32 EntryIndex) const
{
	if (!Entries.IsValidIndex(EntryIndex))
	{
		return false;
	}

	return Entries[EntryIndex].Action == EBlendViewCursorOriginAction::OriginToActive
		? IsActiveTargetEnabled()
		: Entries[EntryIndex].bEnabled;
}

FVector2D SBlendViewPieMenu::ComputeEntrySize(const int32 EntryIndex) const
{
	if (!Entries.IsValidIndex(EntryIndex))
	{
		return FVector2D::ZeroVector;
	}

	const FEntry& Entry = Entries[EntryIndex];
	const FSlateFontInfo FontInfo = ScaledFont(TEXT("NormalFont"));
	const float LabelWidth = MeasureText(Entry.Label, FontInfo).X;
	const float HintWidth = Entry.KeyHint.IsEmpty()
		? 0.0f
		: MeasureText(Entry.KeyHint, FontInfo).X + ButtonHintPadding;
	const float MinWidth = EntryIndex >= 4 ? BottomButtonMinWidth : TopButtonMinWidth;
	const float Width = FMath::Max(
		MinWidth,
		FMath::CeilToFloat(IconColumnWidth + LabelWidth + HintWidth + ButtonRightPadding));
	if (EntryIndex >= 4)
	{
		const bool bLeftColumn = EntryIndex == 4 || EntryIndex == 6;
		const int32 PairedEntryIndex = bLeftColumn
			? (EntryIndex == 4 ? 6 : 4)
			: (EntryIndex == 5 ? 7 : 5);
		if (Entries.IsValidIndex(PairedEntryIndex))
		{
			const FEntry& PairedEntry = Entries[PairedEntryIndex];
			const float PairedLabelWidth = MeasureText(PairedEntry.Label, FontInfo).X;
			const float PairedHintWidth = PairedEntry.KeyHint.IsEmpty()
				? 0.0f
				: MeasureText(PairedEntry.KeyHint, FontInfo).X + ButtonHintPadding;
			const float PairedWidth = FMath::Max(
				MinWidth,
				FMath::CeilToFloat(IconColumnWidth + PairedLabelWidth + PairedHintWidth + ButtonRightPadding));
			return FVector2D(FMath::Max(Width, PairedWidth), BottomCellHeight);
		}
	}
	return FVector2D(Width, EntryIndex >= 4 ? BottomCellHeight : ButtonHeight);
}

FSlateRect SBlendViewPieMenu::ComputeEntryRect(const int32 EntryIndex) const
{
	if (!Entries.IsValidIndex(EntryIndex))
	{
		return FSlateRect();
	}

	const FEntry& Entry = Entries[EntryIndex];
	const FVector2D Size = ComputeEntrySize(EntryIndex);
	FVector2D Position = ScaleMenuPoint(Entry.Position);

	if (EntryIndex == 0 || EntryIndex == 1)
	{
		const float RightEdge = ScaleMenuPoint(Entry.Position + FVector2D(Entry.Size.X, 0.0f)).X;
		Position.X = RightEdge - Size.X;
	}
	else if (EntryIndex >= 4)
	{
		const float Middle = ScaleMenuPoint(FVector2D(
			BottomShellX + 1.0f + BottomCellWidth + BottomCellGap * 0.5f,
			BottomShellY)).X;
		const float Top = ScaleMenuPoint(FVector2D(BottomShellX, BottomShellY + 1.0f)).Y;
		const bool bLowerRow = EntryIndex == 6 || EntryIndex == 7;
		Position.Y = Top + (bLowerRow ? BottomCellHeight + BottomCellGap : 0.0f);
		if (EntryIndex == 4 || EntryIndex == 6)
		{
			Position.X = Middle - BottomCellGap * 0.5f - Size.X;
		}
		else
		{
			Position.X = Middle + BottomCellGap * 0.5f;
		}
	}

	return FSlateRect(
		Position.X,
		Position.Y,
		Position.X + Size.X,
		Position.Y + Size.Y);
}

FSlateRect SBlendViewPieMenu::ComputeBottomShellRect() const
{
	FSlateRect Bounds = ComputeEntryRect(4);
	for (int32 EntryIndex = 5; EntryIndex <= 7; ++EntryIndex)
	{
		const FSlateRect EntryRect = ComputeEntryRect(EntryIndex);
		Bounds.Left = FMath::Min(Bounds.Left, EntryRect.Left);
		Bounds.Top = FMath::Min(Bounds.Top, EntryRect.Top);
		Bounds.Right = FMath::Max(Bounds.Right, EntryRect.Right);
		Bounds.Bottom = FMath::Max(Bounds.Bottom, EntryRect.Bottom);
	}

	return FSlateRect(
		Bounds.Left - 1.0f,
		Bounds.Top - 1.0f,
		Bounds.Right + 1.0f,
		Bounds.Bottom + 1.0f);
}

void SBlendViewPieMenu::PaintEntry(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const ESlateDrawEffect DrawEffects,
	const int32 EntryIndex,
	const int32 CurrentHoveredEntry) const
{
	const FEntry& Entry = Entries[EntryIndex];
	const FVector2D Center = ViewportToLocal(AnchorViewportPosition);
	const FVector2D MenuOrigin = Center - FVector2D(PieMenuWidth * 0.5f, PieRingCenterY);
	const FSlateRect EntryRect = ComputeEntryRect(EntryIndex);
	FVector2D EntryPosition = MenuOrigin + FVector2D(EntryRect.Left, EntryRect.Top);
	const FVector2D EntrySize = EntryRect.GetSize();
	if (EntryIndex < 4 && FSlateApplication::IsInitialized())
	{
		const double Elapsed = FSlateApplication::Get().GetCurrentTime() - OpenTimeSeconds;
		const float RawAlpha = FMath::Clamp(static_cast<float>(Elapsed) / PopupAnimationSeconds, 0.0f, 1.0f);
		const float EaseAlpha = 1.0f - FMath::Pow(1.0f - RawAlpha, 3.0f);
		const FVector2D StartPosition = Center - EntrySize * 0.5f;
		EntryPosition = FMath::Lerp(StartPosition, EntryPosition, EaseAlpha);
	}
	const bool bEntryEnabled = IsEntryEnabled(EntryIndex);
	const bool bHovered = EntryIndex == CurrentHoveredEntry && bEntryEnabled;
	const bool bBottomEntry = EntryIndex >= 4;
	const FLinearColor NormalInnerColor = bBottomEntry ? BottomButtonInnerColor : TopButtonInnerColor;
	const FLinearColor HoverInnerColor = bBottomEntry ? BottomButtonInnerHoverColor : TopButtonInnerHoverColor;
	const FLinearColor InnerColor = bEntryEnabled
		? (bHovered ? HoverInnerColor : NormalInnerColor)
		: ButtonDisabledColor;
	const FLinearColor CurrentTextColor = bEntryEnabled
		? (bHovered ? FLinearColor::White : PieTextColor)
		: PieDisabledTextColor;

	if (EntryIndex < 4)
	{
		DrawLayeredButton(
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			EntryPosition,
			EntrySize,
			DrawEffects,
			InnerColor);
	}
	else
	{
		DrawBox(
			AllottedGeometry,
			OutDrawElements,
			LayerId + 1,
			EntryPosition,
			EntrySize,
			GetBottomInnerBrush(EntryIndex),
			DrawEffects,
			InnerColor);
	}

	const FSlateFontInfo FontInfo = ScaledFont(TEXT("NormalFont"));
	const FVector2D LabelSize = MeasureText(Entry.Label, FontInfo);
	const float ReservedHintWidth = Entry.KeyHint.IsEmpty() ? 7.0f : 22.0f;
	DrawText(
		AllottedGeometry,
		OutDrawElements,
		LayerId + 2,
		EntryPosition + FVector2D(IconColumnWidth, FMath::RoundToFloat((EntrySize.Y - LabelSize.Y) * 0.5f)),
		FVector2D(FMath::Min(LabelSize.X, EntrySize.X - IconColumnWidth - ReservedHintWidth), LabelSize.Y),
		Entry.Label,
		FontInfo,
		DrawEffects,
		CurrentTextColor);

	if (!Entry.KeyHint.IsEmpty())
	{
		const FVector2D HintSize = MeasureText(Entry.KeyHint, FontInfo);
		DrawText(
			AllottedGeometry,
			OutDrawElements,
			LayerId + 2,
			EntryPosition + FVector2D(EntrySize.X - HintSize.X - 8.0f, FMath::RoundToFloat((EntrySize.Y - HintSize.Y) * 0.5f)),
			HintSize,
			Entry.KeyHint,
			FontInfo,
			DrawEffects,
			bEntryEnabled ? PieMutedTextColor : PieDisabledTextColor);
	}

	const FVector2D IconCenter = EntryPosition + FVector2D(19.0f, EntrySize.Y * 0.5f);
	if (const FSlateBrush* IconBrush = GetEntryIconBrush(EntryIndex))
	{
		const FVector2D IconSize(19.0f, 19.0f);
		DrawBox(
			AllottedGeometry,
			OutDrawElements,
			LayerId + 2,
			IconCenter - IconSize * 0.5f,
			IconSize,
			IconBrush,
			DrawEffects,
			CurrentTextColor);
	}
	else if (EntryIndex <= 3)
	{
		DrawLine(AllottedGeometry, OutDrawElements, LayerId + 2, IconCenter + FVector2D(-6.0f, -5.0f), IconCenter + FVector2D(6.0f, 5.0f), DrawEffects, CurrentTextColor, 1.2f);
		DrawLine(AllottedGeometry, OutDrawElements, LayerId + 2, IconCenter + FVector2D(-6.0f, 5.0f), IconCenter + FVector2D(6.0f, -5.0f), DrawEffects, CurrentTextColor, 1.2f);
	}
	else
	{
		DrawLine(AllottedGeometry, OutDrawElements, LayerId + 2, IconCenter + FVector2D(-7.0f, 6.0f), IconCenter + FVector2D(7.0f, 6.0f), DrawEffects, CurrentTextColor, 1.2f);
		DrawLine(AllottedGeometry, OutDrawElements, LayerId + 2, IconCenter + FVector2D(0.0f, -7.0f), IconCenter + FVector2D(0.0f, 6.0f), DrawEffects, CurrentTextColor, 1.2f);
	}
}

void SBlendViewPieMenu::PaintCenter(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const ESlateDrawEffect DrawEffects,
	const FVector2D& MenuOrigin,
	const FVector2D& Center) const
{
	const FSlateFontInfo TitleFont = ScaledFont(TEXT("NormalFont"));
	const FSlateFontInfo GroupFont = ScaledFont(TEXT("NormalFont"));
	const FText Title = FBlendViewLocalization::Text(TEXT("\u6E38\u6807\u4E0E\u539F\u70B9"), TEXT("Cursor and Origin"));
	const FText GroupTitle = FBlendViewLocalization::Text(TEXT("\u7269\u4F53\u539F\u70B9"), TEXT("Object Origin"));
	const FVector2D TitleSize = MeasureText(Title, TitleFont);
	const FVector2D GroupSize = MeasureText(GroupTitle, GroupFont);

	DrawShadowedText(
		AllottedGeometry,
		OutDrawElements,
		LayerId + 1,
		MenuOrigin + FVector2D(
			FMath::RoundToFloat((PieMenuWidth - TitleSize.X) * 0.5f),
			ScaleMenuPoint(FVector2D(PieMenuWidth * 0.5f, 126.0f)).Y),
		TitleSize,
		Title,
		TitleFont,
		DrawEffects,
		PieMutedTextColor);

	constexpr int32 RingSegmentCount = 192;
	constexpr int32 HighlightSegmentCount = 48;
	constexpr float RingInnerRadius = 18.0f;
	constexpr float RingOuterRadius = 30.0f;
	constexpr float RingFeatherSize = 1.25f;
	constexpr float HighlightInnerRadius = 20.0f;
	constexpr float HighlightOuterRadius = 28.0f;
	constexpr float HighlightFeatherSize = 1.0f;
	const FVector2D LocalMouse = ViewportToLocal(PointerViewportPosition);
	const float HighlightAngle = FMath::Atan2(LocalMouse.Y - Center.Y, LocalMouse.X - Center.X);
	constexpr float HighlightArc = UE_PI / 3.8f;
	constexpr float HighlightAngularGap = UE_PI / 38.0f;
	DrawRingSegment(
		AllottedGeometry,
		OutDrawElements,
		LayerId,
		Center,
		RingInnerRadius,
		RingOuterRadius,
		RingFeatherSize,
		0.0f,
		UE_TWO_PI,
		RingSegmentCount,
		DrawEffects,
		PieRingColor);
	DrawRingSegment(
		AllottedGeometry,
		OutDrawElements,
		LayerId + 1,
		Center,
		HighlightInnerRadius,
		HighlightOuterRadius,
		HighlightFeatherSize,
		HighlightAngle - HighlightArc * 0.5f + HighlightAngularGap,
		HighlightAngle + HighlightArc * 0.5f - HighlightAngularGap,
		HighlightSegmentCount,
		DrawEffects,
		PieRingHighlightColor);

	DrawShadowedText(
		AllottedGeometry,
		OutDrawElements,
		LayerId + 1,
		MenuOrigin + FVector2D(
			FMath::RoundToFloat((PieMenuWidth - GroupSize.X) * 0.5f),
			ScaleMenuPoint(FVector2D(PieMenuWidth * 0.5f, 270.0f)).Y),
		GroupSize,
		GroupTitle,
		GroupFont,
		DrawEffects,
		PieMutedTextColor);
}

void SBlendViewPieMenu::PaintTooltip(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const ESlateDrawEffect DrawEffects,
	const int32 CurrentHoveredEntry,
	const FVector2D& MenuOrigin) const
{
	if (!Entries.IsValidIndex(CurrentHoveredEntry))
	{
		return;
	}
	if (CurrentHoveredEntry != HoveredEntry || !FSlateApplication::IsInitialized() ||
		FSlateApplication::Get().GetCurrentTime() - HoverStartTimeSeconds < TooltipDelaySeconds)
	{
		return;
	}

	const FEntry& Entry = Entries[CurrentHoveredEntry];
	if (!IsEntryEnabled(CurrentHoveredEntry) || Entry.Description.IsEmpty())
	{
		return;
	}

	const FSlateFontInfo FontInfo = FAppStyle::GetFontStyle(TEXT("SmallFont"));
	const FVector2D TextSize = MeasureText(Entry.Description, FontInfo);
	const FVector2D Padding(10.0f, 6.0f);
	const FVector2D TooltipSize = TextSize + Padding * 2.0f;
	FVector2D TooltipPosition = ViewportToLocal(PointerViewportPosition) + FVector2D(18.0f, 18.0f);

	const FVector2D MenuMax = MenuOrigin + FVector2D(PieMenuWidth, BottomShellY + BottomCellHeight * 2.0f + 56.0f);
	TooltipPosition.X = FMath::Min(TooltipPosition.X, MenuMax.X - TooltipSize.X);
	TooltipPosition.Y = FMath::Min(TooltipPosition.Y, MenuMax.Y - TooltipSize.Y);

	for (int32 ShadowIndex = 0; ShadowIndex < 4; ++ShadowIndex)
	{
		const float Offset = static_cast<float>(ShadowIndex + 1);
		DrawBox(
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			TooltipPosition + FVector2D(Offset, Offset),
			TooltipSize,
			&TooltipBrush,
			DrawEffects,
			TooltipShadowColor);
	}

	DrawBox(
		AllottedGeometry,
		OutDrawElements,
		LayerId + 1,
		TooltipPosition,
		TooltipSize,
		&TooltipBrush,
		DrawEffects,
		TooltipColor);
	DrawText(
		AllottedGeometry,
		OutDrawElements,
		LayerId + 2,
		TooltipPosition + Padding,
		TextSize,
		Entry.Description,
		FontInfo,
		DrawEffects,
		PieTextColor);
}

FVector2D SBlendViewPieMenu::ViewportToLocal(const FVector2D& ViewportPosition) const
{
	if (!bHasLastPaintGeometry || ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D LocalSize = LastPaintGeometry.GetLocalSize();
	return FVector2D(
		ViewportPosition.X * LocalSize.X / static_cast<double>(ViewportSize.X),
		ViewportPosition.Y * LocalSize.Y / static_cast<double>(ViewportSize.Y));
}

void SBlendViewPieMenu::UpdateHoveredEntryFromLocalPosition(const FVector2D& LocalPosition)
{
	const FVector2D Center = bHasLastPaintGeometry
		? ViewportToLocal(AnchorViewportPosition)
		: FVector2D::ZeroVector;
	const FVector2D MenuOrigin = Center - FVector2D(PieMenuWidth * 0.5f, PieRingCenterY);
	const int32 NewHoveredEntry = FindEntryAt(LocalPosition - MenuOrigin);
	if (NewHoveredEntry != HoveredEntry)
	{
		HoveredEntry = NewHoveredEntry;
		HoverStartTimeSeconds = FSlateApplication::IsInitialized()
			? FSlateApplication::Get().GetCurrentTime()
			: 0.0;
	}
}
