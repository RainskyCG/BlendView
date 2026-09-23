// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewInputTypes.h"
#include "Viewport/BlendViewViewportContext.h"

class FEditorViewportClient;
class FViewport;
class SWidget;

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
	void EndNavigation();

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
	void UpdateOrbit(const FVector2D& CursorDelta) const;
	void UpdatePan(const FVector2D& CursorDelta) const;
	void UpdateDolly(const FVector2D& CursorDelta);
	FVector ResolveAxisViewCenter() const;
	void SnapOrbitToClosestAxis();
	void MarkExternalMovement() const;
	void StopNativeMouseTrackingForAxisSnap() const;

	FEditorViewportClient* CapturedViewportClient = nullptr;
	FViewport* CapturedViewport = nullptr;
	TSharedPtr<SWidget> CapturedViewportLifetimeGuard;
	EBlendViewNavigationMode Mode = EBlendViewNavigationMode::None;
	EBlendViewAxisView AxisView = EBlendViewAxisView::None;
	FVector OrbitPivot = FVector::ZeroVector;
	FVector PanCameraRight = FVector::RightVector;
	FVector PanCameraUp = FVector::UpVector;
	FVector2D PanWorldUnitsPerPixel = FVector2D::ZeroVector;
	FVector DollyPivot = FVector::ZeroVector;
	FVector DollyInitialCameraOffset = FVector::BackwardVector;
	FRotator DollyInitialRotation = FRotator::ZeroRotator;
	double DollyInitialDistance = 0.0;
	double DollyAccumulatedPixels = 0.0;
	FVector CapturedInitialViewLocation = FVector::ZeroVector;
	FVector CapturedInitialLookAtLocation = FVector::ZeroVector;
	FRotator CapturedInitialViewRotation = FRotator::ZeroRotator;
	ELevelViewportType CapturedInitialViewportType = LVT_Perspective;
	float CapturedInitialOrthoZoom = 1.0f;
	bool bUsingOrbitCamera = false;
	bool bPersistentOrbitViewport = false;
	bool bCapturedEditorViewport = false;
	bool bCapturedInitialOrbitCamera = false;
	bool bHasCapturedInitialViewState = false;
};
