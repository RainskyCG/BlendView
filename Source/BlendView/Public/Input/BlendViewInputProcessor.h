// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

class FBlendViewInputRouter;
class FBlendViewQuickFavorites;
class SBlendViewPieMenu;
class SLevelViewport;
class UCanvas;
enum class EBlendViewCursorOriginAction : uint8;
enum class EBlendViewInteractionState : uint8;

class FBlendViewInputProcessor final : public IInputProcessor
{
public:
	FBlendViewInputProcessor();
	virtual ~FBlendViewInputProcessor() override;

	virtual void Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(
		FSlateApplication& SlateApp,
		const FPointerEvent& InWheelEvent,
		const FPointerEvent* InGestureEvent) override;

	void CancelActiveOperation();
	void PrepareForEngineExit();
	void DrawHUD(UCanvas* Canvas);
	void SetBlendViewEnabled(bool bEnabled);
	bool IsBlendViewEnabled() const;
	EBlendViewInteractionState GetState() const;

private:
	void HandleApplicationActivationChanged(bool bIsActive);
	bool TryTogglePieMenu(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	bool TryOpenQuickFavorites(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void OpenPieMenu(FSlateApplication& SlateApp);
	void ClosePieMenu();
	void ExecutePieMenuAction(EBlendViewCursorOriginAction Action);
	void ReleasePieMenuNavigationKeys();
	bool IsPieMenuOpen() const;
	void UpdatePieMenuPointer(const FVector2D& ScreenPosition);

	TUniquePtr<FBlendViewInputRouter> InputRouter;
	TUniquePtr<FBlendViewQuickFavorites> QuickFavorites;
	TSharedPtr<SBlendViewPieMenu> PieMenuWidget;
	TWeakPtr<SLevelViewport> PieMenuLevelViewport;
	TWeakPtr<SWidget> PieMenuViewportWidget;
	FDelegateHandle ApplicationActivationChangedHandle;
	FVector2D PieMenuScreenPosition = FVector2D::ZeroVector;
	FVector2D PieMenuViewportPosition = FVector2D::ZeroVector;
	FIntPoint PieMenuViewportSize = FIntPoint::ZeroValue;
	TOptional<EMouseCursor::Type> LastAppliedHardwareCursorOverride;
	bool bIgnoreNextMouseMoveAfterCursorWrap = false;
};
