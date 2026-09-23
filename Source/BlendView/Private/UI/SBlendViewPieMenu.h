// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Cursor/BlendViewCursorOriginActions.h"
#include "Widgets/SLeafWidget.h"

DECLARE_DELEGATE_OneParam(FBlendViewPieMenuActionDelegate, EBlendViewCursorOriginAction);

class SBlendViewPieMenu final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendViewPieMenu)
	{
	}
		SLATE_EVENT(FSimpleDelegate, OnRequestClose)
		SLATE_EVENT(FBlendViewPieMenuActionDelegate, OnAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetAnchorViewportPosition(const FVector2D& InAnchorViewportPosition, const FIntPoint& InViewportSize);
	void SetPointerViewportPosition(const FVector2D& InPointerViewportPosition);
	bool TryGetActionAtViewportPosition(
		const FVector2D& ViewportPosition,
		EBlendViewCursorOriginAction& OutAction) const;
	bool TryGetHoveredAction(EBlendViewCursorOriginAction& OutAction) const;

	virtual void Tick(
		const FGeometry& AllottedGeometry,
		const double InCurrentTime,
		const float InDeltaTime) override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	struct FEntry
	{
		FText Label;
		FText Description;
		FText KeyHint;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		EBlendViewCursorOriginAction Action = EBlendViewCursorOriginAction::CursorToOrigin;
		bool bEnabled = true;
	};

	int32 FindEntryAt(const FVector2D& LocalPosition) const;
	bool IsEntryEnabled(int32 EntryIndex) const;
	FVector2D ComputeEntrySize(int32 EntryIndex) const;
	FSlateRect ComputeEntryRect(int32 EntryIndex) const;
	FSlateRect ComputeBottomShellRect() const;
	void PaintEntry(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		ESlateDrawEffect DrawEffects,
		int32 EntryIndex,
		int32 CurrentHoveredEntry) const;
	void PaintCenter(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		ESlateDrawEffect DrawEffects,
		const FVector2D& MenuOrigin,
		const FVector2D& Center) const;
	void PaintTooltip(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		ESlateDrawEffect DrawEffects,
		int32 CurrentHoveredEntry,
		const FVector2D& MenuOrigin) const;
	FVector2D ViewportToLocal(const FVector2D& ViewportPosition) const;
	void UpdateHoveredEntryFromLocalPosition(const FVector2D& LocalPosition);

	TArray<FEntry> Entries;
	FSimpleDelegate OnRequestClose;
	FBlendViewPieMenuActionDelegate OnAction;
	FVector2D AnchorViewportPosition = FVector2D::ZeroVector;
	FVector2D PointerViewportPosition = FVector2D::ZeroVector;
	FIntPoint ViewportSize = FIntPoint::ZeroValue;
	mutable FGeometry LastPaintGeometry;
	mutable bool bHasLastPaintGeometry = false;
	double OpenTimeSeconds = 0.0;
	double HoverStartTimeSeconds = 0.0;
	int32 HoveredEntry = INDEX_NONE;
};
