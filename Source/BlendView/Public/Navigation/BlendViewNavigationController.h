// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewInputTypes.h"
#include "Viewport/BlendViewViewportContext.h"

class FEditorViewportClient;
class FLevelEditorViewportClient;
class FViewport;
class SWidget;
class ACameraActor;

enum class EBlendViewNavigationMode : uint8
{
	None,
	Orbit,
	Pan,
	Dolly
};

enum class EBlendViewAxisView : uint8
{
	None,
	Front,
	Back,
	Left,
	Right,
	Top,
	Bottom
};

/**
 * Owns Blender-style viewport navigation state and camera math.
 * The input router decides when navigation may start; this class keeps the
 * captured viewport stable until the middle mouse button is released.
 */
class FBlendViewNavigationController
{
public:
	bool TryBegin(const FBlendViewInputEvent& Event, const FBlendViewViewportContext& ViewportContext);
	EBlendViewInputResult RouteInput(const FBlendViewInputEvent& Event);
	EBlendViewInputResult RoutePersistentAxisViewInput(
		const FBlendViewInputEvent& Event,
		const FBlendViewViewportContext& ViewportContext);
	void EndNavigation();
	void ResetPersistentAxisView();
	void PrepareForEngineExit();

	bool IsNavigating() const { return Mode != EBlendViewNavigationMode::None; }
	EBlendViewNavigationMode GetMode() const { return Mode; }
	EBlendViewAxisView GetAxisView() const { return AxisView; }
	bool IsAxisViewActive() const { return AxisView != EBlendViewAxisView::None; }
	const TCHAR* GetAxisViewName() const;

private:
	void SetCapturedViewportOrbitCameraEnabled(bool bEnabled);
	void InitializeOrbit();
	void InitializePan(const FBlendViewViewportContext& ViewportContext);
	void InitializeDolly();
	void CaptureInitialViewState();
	void RestoreInitialViewState();
	void CancelNavigation();
	void UpdateModeFromModifiers(const FBlendViewInputEvent& Event);
	FBlendViewViewportContext MakeCapturedViewportContext(const FBlendViewInputEvent& Event) const;
	void UpdateOrbit(const FVector2D& CursorDelta, bool bApplyToViewport);
	void ApplyOrbitWorkingView();
	void ReleaseAxisSnapToWorkingView();
	void UpdatePan(const FVector2D& CursorDelta);
	void UpdateDolly(const FVector2D& CursorDelta);
	bool TryResolveOrbitSelectionCenter(FVector& OutCenter) const;
	void SnapOrbitToClosestAxis();
	bool ActivateAxisViewCamera(const FVector& Location, const FRotator& Rotation, float OrthoWidth);
	void DetachAxisViewCameraForOrbit();
	void ReleaseAxisViewCamera(bool bDestroyActor);
	bool IsAxisViewCameraActive(const FEditorViewportClient* ViewportClient) const;
	FLevelEditorViewportClient* GetCapturedLevelViewportClient() const;
	void MarkExternalMovement() const;
	void StopNativeMouseTrackingForAxisSnap() const;

	FEditorViewportClient* CapturedViewportClient = nullptr;
	FViewport* CapturedViewport = nullptr;
	TSharedPtr<SWidget> CapturedViewportLifetimeGuard;
	EBlendViewNavigationMode Mode = EBlendViewNavigationMode::None;
	EBlendViewAxisView AxisView = EBlendViewAxisView::None;
	FVector OrbitPivot = FVector::ZeroVector;
	FVector OrbitWorkingLocation = FVector::ZeroVector;
	FRotator OrbitWorkingRotation = FRotator::ZeroRotator;
	FVector PanCameraRight = FVector::RightVector;
	FVector PanCameraUp = FVector::UpVector;
	FVector2D PanWorldUnitsPerPixel = FVector2D::ZeroVector;
	FVector DollyPivot = FVector::ZeroVector;
	FVector DollyInitialCameraOffset = FVector::BackwardVector;
	FRotator DollyInitialRotation = FRotator::ZeroRotator;
	double DollyInitialDistance = 0.0;
	double DollyAccumulatedPixels = 0.0;
	float DollyInitialOrthoWidth = 0.0f;
	FVector CapturedInitialViewLocation = FVector::ZeroVector;
	FVector CapturedInitialLookAtLocation = FVector::ZeroVector;
	FRotator CapturedInitialViewRotation = FRotator::ZeroRotator;
	ELevelViewportType CapturedInitialViewportType = LVT_Perspective;
	float CapturedInitialOrthoZoom = 1.0f;
	FTransform CapturedInitialAxisCameraTransform = FTransform::Identity;
	float CapturedInitialAxisCameraOrthoWidth = 0.0f;
	FVector CapturedInitialAxisViewPivot = FVector::ZeroVector;
	FVector PersistentAxisViewPivot = FVector::ZeroVector;
	TWeakObjectPtr<ACameraActor> AxisViewCamera;
	FLevelEditorViewportClient* AxisViewViewportClient = nullptr;
	TWeakPtr<SWidget> AxisViewViewportLifetimeGuard;
	bool bUsingOrbitCamera = false;
	bool bPersistentOrbitViewport = false;
	bool bCapturedLevelEditorViewport = false;
	bool bCapturedInitialOrbitCamera = false;
	bool bHasCapturedInitialViewState = false;
	bool bCapturedInitialAxisCameraActive = false;
	bool bHasOrbitWorkingView = false;
	bool bAxisSnapModifierActive = false;
};
