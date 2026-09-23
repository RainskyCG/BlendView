// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewFlightInputState.h"
#include "Core/BlendViewInputTypes.h"
#include "Core/BlendViewStateMachine.h"
#include "Core/BlendViewToolManager.h"
#include "Cursor/BlendViewCursorOriginActions.h"
#include "Cursor/BlendViewSceneCursor.h"
#include "Graph/BlendViewGraphActionController.h"
#include "Graph/BlendViewGraphTransformController.h"
#include "Navigation/BlendViewNavigationController.h"
#include "Navigation/BlendViewFlightSpeedBoostSession.h"
#include "Viewport/BlendViewViewportContext.h"

class FEditorViewportClient;
class FSceneViewport;
class FUICommandInfo;
class SWidget;
class UBlendViewSettings;
enum class EBlendViewTransformMode : uint8;

class FBlendViewInputRouter
{
public:
	EBlendViewInputResult RouteInput(const FBlendViewInputEvent& Event);
	bool RouteFlightNavigationKeyDown(const FKeyEvent& KeyEvent);
	bool RouteFlightNavigationKeyUp(const FKeyEvent& KeyEvent);
	void Tick(float DeltaTime);
	void DrawHUD(UCanvas* Canvas);
	void CancelActiveOperation();
	void PrepareForEngineExit();
	void SetBlendViewEnabled(bool bEnabled);
	bool ExecuteCursorOriginAction(EBlendViewCursorOriginAction Action);

	EBlendViewInteractionState GetState() const { return StateMachine.GetState(); }
	bool IsBlendViewEnabled() const { return bBlendViewEnabled; }
	TOptional<EMouseCursor::Type> GetHardwareCursorOverride() const
	{
		return GraphTransformController.GetHardwareCursorOverride();
	}

private:
	bool IsPointerEvent(const FBlendViewInputEvent& Event) const;
	bool IsModalCancelInput(const FBlendViewInputEvent& Event) const;
	bool IsModalConfirmInput(const FBlendViewInputEvent& Event) const;
	bool IsTransformResetInput(const FBlendViewInputEvent& Event, EBlendViewTransformResetChannel& OutChannel) const;
	bool TryResetCurrentTransform(EBlendViewTransformResetChannel Channel);
	bool ResetSelectedTransform(EBlendViewTransformMode Mode);
	bool ResetSelectedActorTransform(EBlendViewTransformMode Mode);
	bool ResetSelectedNativeComponentTransform(EBlendViewTransformMode Mode);
	bool TryFrameSelectedUnderCursor(const FBlendViewInputEvent& Event);
	bool FrameSelectedInViewport() const;
	bool TryOpenMoveSelectedToFolderMenu(const FBlendViewInputEvent& Event);
	EBlendViewInputResult TryBeginDuplicateTranslateTool(const FBlendViewInputEvent& Event);
	EBlendViewInputResult TryBeginTransformTool(const FBlendViewInputEvent& Event);
	bool ShouldBlockNewBlendViewShortcut() const;
	bool IsViewportMouseButtonDown(const FKey& Key) const;
	bool IsFlightNavigationButtonDownInViewport() const;
	bool IsMouseNavigationButtonDownInViewport() const;
	void SyncFlightNavigationButtonState();
	bool IsFlightNavigationInputActive() const;
	void CaptureFlightNavigationViewport(const FBlendViewInputEvent& Event);
	void SuppressViewportAltStateForFlight();
	bool EnsureFlightNavigationSession(float SpeedMultiplier);
	void EndFlightNavigationSession();
	void UpdateNavigationSpeedBoost(const FBlendViewInputEvent& Event, const UBlendViewSettings* Settings);
	bool RouteFlightNavigationKey(const FKeyEvent& KeyEvent, bool bPressed);
	void RefreshNativeFlightNavigationKeysForSpeedModifier();
	void SyncFlightNavigationState();
	void UpdateFlightNavigationSession();
	void UpdateNavigationStatusBar(const UBlendViewSettings* Settings);
	void ReleasePointerState();
	bool TryBeginCursorPlacement(const FBlendViewInputEvent& Event);
	bool UpdateCursorPlacement(const FBlendViewInputEvent& Event);
	EBlendViewInputResult TryCommitCursorPlacement(const FBlendViewInputEvent& Event);
	void CancelCursorPlacement();
	bool SetCursorFromViewportClick(const FBlendViewViewportContext& Context, const FVector2D& ViewportPosition);
	void RestorePointerAfterCursorPlacement(const FBlendViewViewportContext& Context);

	FBlendViewStateMachine StateMachine;
	FBlendViewToolManager ToolManager;
	FBlendViewGraphActionController GraphActionController;
	FBlendViewGraphTransformController GraphTransformController;
	FBlendViewNavigationController NavigationController;
	FBlendViewViewportResolver ViewportResolver;
	FBlendViewViewportContext LastViewportContext;
	FBlendViewViewportContext FlightNavigationContext;
	FBlendViewFlightInputState FlightInputState;
	FBlendViewFlightSpeedBoostSession FlightSpeedBoostSession;
	FSceneViewport* FlightNavigationViewport = nullptr;
	TSharedPtr<SWidget> FlightNavigationLifetimeGuard;
	bool bBlendViewEnabled = true;
	bool bSuppressFlightAltUntilKeyUp = false;
	bool bSuppressNextRightMouseUp = false;
	bool bSuppressPointerInputUntilRightMouseUp = false;
	bool bNavigationReleasePassThrough = true;
	bool bPendingCursorPlacement = false;
	FBlendViewSceneCursor SceneCursor;
	FVector2D CursorPlacementStartScreenPosition = FVector2D::ZeroVector;
	FBlendViewViewportContext CursorPlacementContext;
};
