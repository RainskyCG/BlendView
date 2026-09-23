// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Navigation/BlendViewNavigationController.h"

#include "BlendViewSettings.h"
#include "Compat/BlendViewViewportCompat.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "MouseDeltaTracker.h"
#include "SceneView.h"

namespace
{
	constexpr float OrbitDegreesPerPixel = 0.25f;
	constexpr float MinimumReferenceDistance = 10.0f;
	constexpr float DefaultReferenceDistance = 1000.0f;
	constexpr float FallbackPanDistancePerPixel = 0.001f;
	constexpr double DollyZoomRatePerPixel = 0.01;
	constexpr double MinimumDollyDistance = 1.0;
	constexpr double MaximumDollyExponent = 20.0;

	float GetReferenceDistance(const FEditorViewportClient* ViewportClient)
	{
		const float Distance = FVector::Distance(
			ViewportClient->GetViewLocation(),
			ViewportClient->GetLookAtLocation());
		return Distance >= MinimumReferenceDistance ? Distance : DefaultReferenceDistance;
	}

	float GetOrbitMouseSensitivity()
	{
		const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
		return Settings
			? FMath::Clamp(Settings->OrbitSensitivity, 0.01f, 1.0f)
			: OrbitDegreesPerPixel;
	}

	float GetOrbitPitchDirection()
	{
		const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
		return Settings && Settings->bInvertOrbitYAxis ? -1.0f : 1.0f;
	}

	struct FBlendViewAxisSnapViewMatch
	{
		bool bValid = false;
		float OrthoZoom = 0.0f;
		FVector2D PivotPixelOffsetFromCenter = FVector2D::ZeroVector;
	};

	float GetEditorOrthoZoomFactor(const float ViewportWidth)
	{
		const IConsoleVariable* AlignedOrthoZoomCVar =
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.AlignedOrthoZoom"));
		if (AlignedOrthoZoomCVar && AlignedOrthoZoomCVar->GetInt() != 0)
		{
			return ViewportWidth / 500.0f;
		}
		return 1.0f;
	}

	FBlendViewAxisSnapViewMatch CaptureAxisSnapViewMatch(
		FEditorViewportClient* ViewportClient,
		FViewport* Viewport,
		const FVector& Pivot)
	{
		FBlendViewAxisSnapViewMatch Match;
		if (!ViewportClient || !Viewport || !ViewportClient->IsPerspective())
		{
			return Match;
		}

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			Viewport,
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags));
		const FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		if (!View || !View->IsPerspectiveProjection())
		{
			return Match;
		}

		const float ViewportWidth = static_cast<float>(View->UnscaledViewRect.Width());
		const float ViewportHeight = static_cast<float>(View->UnscaledViewRect.Height());
		if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
		{
			return Match;
		}

		const FMatrix& ProjectionMatrix = BlendViewViewportCompat::GetProjectionMatrix(*View);
		const float ProjectionX = FMath::Abs(static_cast<float>(ProjectionMatrix.M[0][0]));
		const float ProjectionY = FMath::Abs(static_cast<float>(ProjectionMatrix.M[1][1]));
		if (ProjectionX <= UE_SMALL_NUMBER || ProjectionY <= UE_SMALL_NUMBER)
		{
			return Match;
		}

		const FVector ViewLocation = ViewportClient->GetViewLocation();
		const FVector ViewForward = ViewportClient->GetViewRotation().Vector().GetSafeNormal();
		float PivotDepth = FVector::DotProduct(Pivot - ViewLocation, ViewForward);
		if (PivotDepth < MinimumReferenceDistance)
		{
			PivotDepth = FVector::Distance(Pivot, ViewLocation);
		}
		if (PivotDepth < MinimumReferenceDistance)
		{
			PivotDepth = GetReferenceDistance(ViewportClient);
		}

		FVector2D PivotPixel = FVector2D::ZeroVector;
		if (!View->WorldToPixel(Pivot, PivotPixel))
		{
			return Match;
		}

		const FVector2D ViewCenter(
			static_cast<float>(View->UnscaledViewRect.Min.X) + ViewportWidth * 0.5f,
			static_cast<float>(View->UnscaledViewRect.Min.Y) + ViewportHeight * 0.5f);
		const float UnitsPerPixelX = 2.0f * PivotDepth / (ViewportWidth * ProjectionX);
		const float UnitsPerPixelY = 2.0f * PivotDepth / (ViewportHeight * ProjectionY);
		const float UnitsPerPixel = (UnitsPerPixelX + UnitsPerPixelY) * 0.5f;
		if (UnitsPerPixel <= UE_SMALL_NUMBER)
		{
			return Match;
		}

		Match.bValid = true;
		Match.PivotPixelOffsetFromCenter = PivotPixel - ViewCenter;
		Match.OrthoZoom = UnitsPerPixel * ViewportWidth * 15.0f / GetEditorOrthoZoomFactor(ViewportWidth);
		return Match;
	}

	void ApplyAxisSnapViewMatch(
		FEditorViewportClient* ViewportClient,
		FViewport* Viewport,
		const FVector& Pivot,
		const FBlendViewAxisSnapViewMatch& Match)
	{
		if (!Match.bValid || !ViewportClient || !Viewport)
		{
			return;
		}

		const float OrthoZoom = FMath::Clamp<float>(
			Match.OrthoZoom,
			MIN_ORTHOZOOM,
			MAX_ORTHOZOOM);
		ViewportClient->SetOrthoZoom(OrthoZoom);

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			Viewport,
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags));
		const FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		if (!View || View->IsPerspectiveProjection())
		{
			return;
		}

		const float UnitsPerPixel = ViewportClient->GetOrthoUnitsPerPixel(Viewport);
		const FVector ViewRight = View->GetViewRight().GetSafeNormal();
		const FVector ViewUp = View->GetViewUp().GetSafeNormal();
		if (UnitsPerPixel <= UE_SMALL_NUMBER ||
			ViewRight.IsNearlyZero() ||
			ViewUp.IsNearlyZero())
		{
			return;
		}

		const FVector MatchedViewLocation =
			Pivot -
			ViewRight * Match.PivotPixelOffsetFromCenter.X * UnitsPerPixel +
			ViewUp * Match.PivotPixelOffsetFromCenter.Y * UnitsPerPixel;
		ViewportClient->SetViewLocation(MatchedViewLocation);
	}

	struct FBlendViewAxisSnapCandidate
	{
		EBlendViewAxisView AxisView = EBlendViewAxisView::None;
		ELevelViewportType ViewportType = LVT_Perspective;
		double Score = -TNumericLimits<double>::Max();
	};

	FBlendViewAxisSnapCandidate FindClosestAxisSnapCandidate(const FRotator& CurrentRotation)
	{
		struct FAxisDefinition
		{
			EBlendViewAxisView AxisView;
			ELevelViewportType ViewportType;
			FVector Forward;
		};

		static const FAxisDefinition AxisDefinitions[] =
		{
			{EBlendViewAxisView::Front, LVT_OrthoFront, FVector::BackwardVector},
			{EBlendViewAxisView::Back, LVT_OrthoBack, FVector::ForwardVector},
			{EBlendViewAxisView::Left, LVT_OrthoLeft, FVector::LeftVector},
			{EBlendViewAxisView::Right, LVT_OrthoRight, FVector::RightVector},
			{EBlendViewAxisView::Top, LVT_OrthoTop, FVector::DownVector},
			{EBlendViewAxisView::Bottom, LVT_OrthoBottom, FVector::UpVector}
		};

		const FRotationMatrix CurrentMatrix(CurrentRotation);
		const FVector CurrentForward = CurrentMatrix.GetUnitAxis(EAxis::X).GetSafeNormal();

		FBlendViewAxisSnapCandidate Best;
		for (const FAxisDefinition& Axis : AxisDefinitions)
		{
			const FVector AxisForward = Axis.Forward.GetSafeNormal();
			const double Score = FVector::DotProduct(CurrentForward, AxisForward);
			if (Score > Best.Score)
			{
				Best.AxisView = Axis.AxisView;
				Best.ViewportType = Axis.ViewportType;
				Best.Score = Score;
			}
		}

		return Best;
	}
}

bool FBlendViewNavigationController::TryBegin(
	const FBlendViewInputEvent& Event,
	const FBlendViewViewportContext& ViewportContext)
{
	if (IsNavigating() ||
		Event.Type != EBlendViewInputEventType::MouseDown ||
		Event.Key != EKeys::MiddleMouseButton ||
		!ViewportContext.IsValid() ||
		!ViewportContext.ViewportClient)
	{
		return false;
	}

	CapturedViewportLifetimeGuard = ViewportContext.ViewportWidget.Pin();
	if (!CapturedViewportLifetimeGuard.IsValid())
	{
		return false;
	}
	CapturedViewportClient = ViewportContext.ViewportClient;
	CapturedViewport = ViewportContext.Viewport;
	bCapturedEditorViewport = ViewportContext.Kind == EBlendViewViewportKind::EditorViewport;
	bPersistentOrbitViewport = CapturedViewportClient->ShouldOrbitCamera();
	bCapturedInitialOrbitCamera = CapturedViewportClient->bUsingOrbitCamera;
	CaptureInitialViewState();
	const bool bBeginPan = Event.bShiftDown && !Event.bControlDown;
	if (!bBeginPan && !CapturedViewportClient->IsPerspective())
	{
		CapturedViewportClient->SetViewportType(LVT_Perspective);
	}
	bUsingOrbitCamera = CapturedViewportClient->bUsingOrbitCamera;

	if (Event.bControlDown)
	{
		Mode = EBlendViewNavigationMode::Dolly;
		InitializeDolly();
	}
	else if (Event.bShiftDown)
	{
		Mode = EBlendViewNavigationMode::Pan;
		InitializePan(ViewportContext);
	}
	else
	{
		InitializeOrbit();
	}

	MarkExternalMovement();
	if (Mode == EBlendViewNavigationMode::Orbit && Event.bAltDown)
	{
		SnapOrbitToClosestAxis();
	}
	return true;
}

EBlendViewInputResult FBlendViewNavigationController::RouteInput(const FBlendViewInputEvent& Event)
{
	if (!IsNavigating())
	{
		return EBlendViewInputResult::PassThrough;
	}

	if (Event.Type == EBlendViewInputEventType::MouseUp && Event.Key == EKeys::MiddleMouseButton)
	{
		EndNavigation();
		return EBlendViewInputResult::PassThrough;
	}

	if ((Event.Type == EBlendViewInputEventType::KeyDown && Event.Key == EKeys::Escape) ||
		(Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::RightMouseButton))
	{
		CancelNavigation();
		return EBlendViewInputResult::Handled;
	}

	if (Mode == EBlendViewNavigationMode::Orbit &&
		Event.Type == EBlendViewInputEventType::KeyDown &&
		(Event.Key == EKeys::LeftAlt || Event.Key == EKeys::RightAlt))
	{
		SnapOrbitToClosestAxis();
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::KeyDown ||
		Event.Type == EBlendViewInputEventType::KeyUp ||
		Event.Type == EBlendViewInputEventType::MouseMove)
	{
		UpdateModeFromModifiers(Event);
	}

	if (Event.Type == EBlendViewInputEventType::MouseMove && !Event.CursorDelta.IsNearlyZero())
	{
		if (AxisView != EBlendViewAxisView::None)
		{
			return EBlendViewInputResult::Handled;
		}

		switch (Mode)
		{
		case EBlendViewNavigationMode::Orbit:
			UpdateOrbit(Event.CursorDelta);
			break;
		case EBlendViewNavigationMode::Pan:
			UpdatePan(Event.CursorDelta);
			break;
		case EBlendViewNavigationMode::Dolly:
			UpdateDolly(Event.CursorDelta);
			break;
		default:
			break;
		}
		return EBlendViewInputResult::Handled;
	}

	return EBlendViewInputResult::Handled;
}

void FBlendViewNavigationController::EndNavigation()
{
	if (CapturedViewportClient)
	{
		const bool bSnappedToAxisView = AxisView != EBlendViewAxisView::None;
		SetCapturedViewportOrbitCameraEnabled(bSnappedToAxisView ? false : bCapturedInitialOrbitCamera);
	}

	Mode = EBlendViewNavigationMode::None;
	AxisView = EBlendViewAxisView::None;
	CapturedViewportClient = nullptr;
	CapturedViewport = nullptr;
	CapturedViewportLifetimeGuard.Reset();
	OrbitPivot = FVector::ZeroVector;
	PanCameraRight = FVector::RightVector;
	PanCameraUp = FVector::UpVector;
	PanWorldUnitsPerPixel = FVector2D::ZeroVector;
	DollyPivot = FVector::ZeroVector;
	DollyInitialCameraOffset = FVector::BackwardVector;
	DollyInitialRotation = FRotator::ZeroRotator;
	DollyInitialDistance = 0.0;
	DollyAccumulatedPixels = 0.0;
	bUsingOrbitCamera = false;
	bPersistentOrbitViewport = false;
	bCapturedEditorViewport = false;
	bCapturedInitialOrbitCamera = false;
	bHasCapturedInitialViewState = false;
}

void FBlendViewNavigationController::CaptureInitialViewState()
{
	if (!CapturedViewportClient)
	{
		bHasCapturedInitialViewState = false;
		return;
	}

	CapturedInitialViewLocation = CapturedViewportClient->GetViewLocation();
	CapturedInitialViewRotation = CapturedViewportClient->GetViewRotation();
	CapturedInitialLookAtLocation = CapturedViewportClient->GetLookAtLocation();
	CapturedInitialViewportType = CapturedViewportClient->GetViewportType();
	CapturedInitialOrthoZoom = CapturedViewportClient->GetOrthoZoom();
	bHasCapturedInitialViewState = true;
}

void FBlendViewNavigationController::RestoreInitialViewState()
{
	if (!CapturedViewportClient || !bHasCapturedInitialViewState)
	{
		return;
	}

	CapturedViewportClient->SetViewportType(CapturedInitialViewportType);
	SetCapturedViewportOrbitCameraEnabled(bCapturedInitialOrbitCamera);
	CapturedViewportClient->SetViewLocation(CapturedInitialViewLocation);
	CapturedViewportClient->SetViewRotation(CapturedInitialViewRotation);
	CapturedViewportClient->SetLookAtLocation(CapturedInitialLookAtLocation, false);
	if (CapturedInitialOrthoZoom > 0.0f)
	{
		CapturedViewportClient->SetOrthoZoom(CapturedInitialOrthoZoom);
	}
	CapturedViewportClient->Invalidate();
}

void FBlendViewNavigationController::CancelNavigation()
{
	RestoreInitialViewState();
	AxisView = EBlendViewAxisView::None;
	EndNavigation();
}

const TCHAR* FBlendViewNavigationController::GetAxisViewName() const
{
	switch (AxisView)
	{
	case EBlendViewAxisView::Front:
		return TEXT("前视图");
	case EBlendViewAxisView::Back:
		return TEXT("后视图");
	case EBlendViewAxisView::Left:
		return TEXT("左视图");
	case EBlendViewAxisView::Right:
		return TEXT("右视图");
	case EBlendViewAxisView::Top:
		return TEXT("上视图");
	case EBlendViewAxisView::Bottom:
		return TEXT("下视图");
	default:
	return TEXT("");
	}
}

void FBlendViewNavigationController::SetCapturedViewportOrbitCameraEnabled(const bool bEnabled)
{
	if (!CapturedViewportClient)
	{
		return;
	}

	const FVector CameraLocation = CapturedViewportClient->GetViewLocation();
	const FVector LookAtLocation = CapturedViewportClient->GetLookAtLocation();
	if (FVector::Distance(CameraLocation, LookAtLocation) < MinimumReferenceDistance)
	{
		CapturedViewportClient->SetLookAtLocation(
			CameraLocation + CapturedViewportClient->GetViewRotation().Vector() * DefaultReferenceDistance,
			false);
	}

	if (CapturedViewportClient->bUsingOrbitCamera != bEnabled)
	{
		CapturedViewportClient->ToggleOrbitCamera(bEnabled);
	}
	bUsingOrbitCamera = CapturedViewportClient->bUsingOrbitCamera;
}

void FBlendViewNavigationController::InitializeOrbit()
{
	if (!CapturedViewportClient)
	{
		return;
	}

	SetCapturedViewportOrbitCameraEnabled(bPersistentOrbitViewport);

	Mode = EBlendViewNavigationMode::Orbit;
	AxisView = EBlendViewAxisView::None;
	OrbitPivot = CapturedViewportClient->GetLookAtLocation();

	const FVector CameraLocation = CapturedViewportClient->GetViewLocation();
	if (FVector::Distance(CameraLocation, OrbitPivot) < MinimumReferenceDistance)
	{
		OrbitPivot = CameraLocation +
			CapturedViewportClient->GetViewRotation().Vector() * DefaultReferenceDistance;
	}
}

void FBlendViewNavigationController::InitializePan(const FBlendViewViewportContext& ViewportContext)
{
	if (!CapturedViewportClient || !CapturedViewport)
	{
		return;
	}

	Mode = EBlendViewNavigationMode::Pan;
	AxisView = EBlendViewAxisView::None;
	SetCapturedViewportOrbitCameraEnabled(false);

	if (!CapturedViewportClient->IsPerspective())
	{
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			CapturedViewport,
			CapturedViewportClient->GetScene(),
			CapturedViewportClient->EngineShowFlags));
		if (const FSceneView* View = CapturedViewportClient->CalcSceneView(&ViewFamily))
		{
			PanCameraRight = View->GetViewRight().GetSafeNormal();
			PanCameraUp = View->GetViewUp().GetSafeNormal();
		}
		PanWorldUnitsPerPixel = FVector2D(
			CapturedViewportClient->GetOrthoUnitsPerPixel(CapturedViewport));
		return;
	}

	const FRotator CameraRotation = CapturedViewportClient->GetViewRotation();
	const FRotationMatrix CameraMatrix(CameraRotation);
	PanCameraRight = CameraMatrix.GetUnitAxis(EAxis::Y);
	PanCameraUp = CameraMatrix.GetUnitAxis(EAxis::Z);

	FVector ReferenceLocation = CapturedViewportClient->GetLookAtLocation();
	const int32 MouseX = FMath::RoundToInt(ViewportContext.MouseViewportPosition.X);
	const int32 MouseY = FMath::RoundToInt(ViewportContext.MouseViewportPosition.Y);
	if (HHitProxy* HitProxy = CapturedViewport->GetHitProxy(MouseX, MouseY);
		HitProxy && HitProxy->IsA(HActor::StaticGetType()))
	{
		ReferenceLocation = CapturedViewportClient->GetHitProxyObjectLocation(MouseX, MouseY);
	}

	const FVector CameraLocation = CapturedViewportClient->GetViewLocation();
	const FVector CameraForward = CameraRotation.Vector();
	float ReferenceDepth = FVector::DotProduct(ReferenceLocation - CameraLocation, CameraForward);
	if (ReferenceDepth < MinimumReferenceDistance)
	{
		ReferenceDepth = FVector::DotProduct(
			CapturedViewportClient->GetLookAtLocation() - CameraLocation,
			CameraForward);
	}
	if (ReferenceDepth < MinimumReferenceDistance)
	{
		ReferenceDepth = GetReferenceDistance(CapturedViewportClient);
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		CapturedViewport,
		CapturedViewportClient->GetScene(),
		CapturedViewportClient->EngineShowFlags));
	const FSceneView* View = CapturedViewportClient->CalcSceneView(&ViewFamily);
	if (View && View->IsPerspectiveProjection())
	{
		const float ViewportWidth = static_cast<float>(View->UnscaledViewRect.Width());
		const float ViewportHeight = static_cast<float>(View->UnscaledViewRect.Height());
		const FMatrix& ProjectionMatrix = BlendViewViewportCompat::GetProjectionMatrix(*View);
		const float ProjectionX = FMath::Abs(static_cast<float>(ProjectionMatrix.M[0][0]));
		const float ProjectionY = FMath::Abs(static_cast<float>(ProjectionMatrix.M[1][1]));
		if (ViewportWidth > 0.0f && ViewportHeight > 0.0f &&
			ProjectionX > UE_SMALL_NUMBER && ProjectionY > UE_SMALL_NUMBER)
		{
			PanWorldUnitsPerPixel.X = 2.0f * ReferenceDepth / (ViewportWidth * ProjectionX);
			PanWorldUnitsPerPixel.Y = 2.0f * ReferenceDepth / (ViewportHeight * ProjectionY);
			return;
		}
	}

	const float FallbackScale = ReferenceDepth * FallbackPanDistancePerPixel;
	PanWorldUnitsPerPixel = FVector2D(FallbackScale, FallbackScale);
}

void FBlendViewNavigationController::InitializeDolly()
{
	if (!CapturedViewportClient)
	{
		return;
	}

	Mode = EBlendViewNavigationMode::Dolly;
	AxisView = EBlendViewAxisView::None;
	SetCapturedViewportOrbitCameraEnabled(false);

	const FVector CameraLocation = CapturedViewportClient->GetViewLocation();
	DollyPivot = CapturedViewportClient->GetLookAtLocation();
	DollyInitialCameraOffset = CameraLocation - DollyPivot;
	DollyInitialDistance = DollyInitialCameraOffset.Length();
	DollyInitialRotation = CapturedViewportClient->GetViewRotation();
	DollyAccumulatedPixels = 0.0;

	if (DollyInitialDistance < MinimumReferenceDistance)
	{
		DollyPivot = CameraLocation + DollyInitialRotation.Vector() * DefaultReferenceDistance;
		DollyInitialCameraOffset = CameraLocation - DollyPivot;
		DollyInitialDistance = DollyInitialCameraOffset.Length();
	}
}

void FBlendViewNavigationController::UpdateModeFromModifiers(const FBlendViewInputEvent& Event)
{
	if (!CapturedViewportClient || !CapturedViewport)
	{
		return;
	}

	const bool bShiftActive =
		Event.bShiftDown ||
		(Event.Type == EBlendViewInputEventType::KeyDown &&
			(Event.Key == EKeys::LeftShift || Event.Key == EKeys::RightShift));
	const bool bControlActive =
		Event.bControlDown ||
		(Event.Type == EBlendViewInputEventType::KeyDown &&
			(Event.Key == EKeys::LeftControl || Event.Key == EKeys::RightControl));

	const EBlendViewNavigationMode DesiredMode = bControlActive
		? EBlendViewNavigationMode::Dolly
		: (bShiftActive ? EBlendViewNavigationMode::Pan : EBlendViewNavigationMode::Orbit);

	if (DesiredMode == Mode)
	{
		return;
	}

	switch (DesiredMode)
	{
	case EBlendViewNavigationMode::Pan:
		InitializePan(MakeCapturedViewportContext(Event));
		break;
	case EBlendViewNavigationMode::Dolly:
		InitializeDolly();
		break;
	case EBlendViewNavigationMode::Orbit:
		InitializeOrbit();
		break;
	default:
		break;
	}
}

FBlendViewViewportContext FBlendViewNavigationController::MakeCapturedViewportContext(
	const FBlendViewInputEvent& Event) const
{
	FBlendViewViewportContext Context;
	Context.ViewportClient = CapturedViewportClient;
	Context.Viewport = CapturedViewport;
	Context.ViewportWidget = CapturedViewportLifetimeGuard;
	Context.ViewportSize = CapturedViewport ? CapturedViewport->GetSizeXY() : FIntPoint::ZeroValue;

	FVector2D ScreenPosition = Event.ScreenPosition;
	if (ScreenPosition.IsNearlyZero() && FSlateApplication::IsInitialized())
	{
		ScreenPosition = FSlateApplication::Get().GetCursorPos();
	}
	Context.MouseScreenPosition = ScreenPosition;

	if (CapturedViewport)
	{
		const FIntPoint ScreenPixel(
			FMath::RoundToInt(ScreenPosition.X),
			FMath::RoundToInt(ScreenPosition.Y));
		const FVector2D NormalizedPosition = CapturedViewport->VirtualDesktopPixelToViewport(ScreenPixel);
		Context.MouseViewportPosition = FVector2D(Context.ViewportSize) * NormalizedPosition;
	}

	return Context;
}

void FBlendViewNavigationController::UpdateOrbit(const FVector2D& CursorDelta) const
{
	if (!CapturedViewportClient)
	{
		return;
	}

	MarkExternalMovement();

	const FVector CameraLocation = CapturedViewportClient->GetViewLocation();
	const FRotator CameraRotation = CapturedViewportClient->GetViewRotation();

	const float OrbitSensitivity = GetOrbitMouseSensitivity();
	const float DeltaYaw = CursorDelta.X * OrbitSensitivity;
	if (bPersistentOrbitViewport)
	{
		FRotator OrbitRotation = CameraRotation;
		OrbitRotation.Yaw -= DeltaYaw;
		OrbitRotation.Pitch = FMath::Clamp(
			OrbitRotation.Pitch -
				CursorDelta.Y * OrbitSensitivity * GetOrbitPitchDirection(),
			-89.0f,
			89.0f);
		OrbitRotation.Roll = 0.0f;

		CapturedViewportClient->SetLookAtLocation(OrbitPivot, false);
		CapturedViewportClient->SetViewRotation(OrbitRotation);
		CapturedViewportClient->SetViewLocation(
			CapturedViewportClient->GetViewTransform().ComputeOrbitMatrix().Inverse().GetOrigin());
		CapturedViewportClient->Invalidate();
		return;
	}

	FVector PivotToCamera = CameraLocation - OrbitPivot;
	const float RequestedPitch =
		CameraRotation.Pitch - CursorDelta.Y * OrbitSensitivity * GetOrbitPitchDirection();
	const float NewPitch = FMath::Clamp(RequestedPitch, -89.0f, 89.0f);
	const float AppliedPitch = CameraRotation.Pitch - NewPitch;

	PivotToCamera = FRotator(0.0f, DeltaYaw, 0.0f).RotateVector(PivotToCamera);
	const FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
	PivotToCamera = FQuat(CameraRight, FMath::DegreesToRadians(AppliedPitch)).RotateVector(PivotToCamera);

	FRotator NewRotation = CameraRotation;
	NewRotation.Yaw += DeltaYaw;
	NewRotation.Pitch = NewPitch;
	NewRotation.Roll = 0.0f;

	const FVector NewLocation = OrbitPivot + PivotToCamera;
	CapturedViewportClient->SetViewLocation(NewLocation);
	CapturedViewportClient->SetViewRotation(NewRotation);
	CapturedViewportClient->SetLookAtLocation(
		NewLocation + NewRotation.Vector() * FVector::Distance(NewLocation, OrbitPivot));
	CapturedViewportClient->Invalidate();
}

void FBlendViewNavigationController::UpdatePan(const FVector2D& CursorDelta) const
{
	if (!CapturedViewportClient)
	{
		return;
	}

	MarkExternalMovement();

	const FVector PanDelta =
		-PanCameraRight * CursorDelta.X * PanWorldUnitsPerPixel.X +
		PanCameraUp * CursorDelta.Y * PanWorldUnitsPerPixel.Y;

	CapturedViewportClient->SetLookAtLocation(CapturedViewportClient->GetLookAtLocation() + PanDelta);
	if (bUsingOrbitCamera)
	{
		CapturedViewportClient->SetViewLocation(
			CapturedViewportClient->GetViewTransform().ComputeOrbitMatrix().Inverse().GetOrigin());
	}
	else
	{
		CapturedViewportClient->SetViewLocation(CapturedViewportClient->GetViewLocation() + PanDelta);
	}
	CapturedViewportClient->Invalidate();
}

void FBlendViewNavigationController::UpdateDolly(const FVector2D& CursorDelta)
{
	if (!CapturedViewportClient || DollyInitialDistance < MinimumDollyDistance)
	{
		return;
	}

	MarkExternalMovement();

	const double MaximumAccumulatedPixels = MaximumDollyExponent / DollyZoomRatePerPixel;
	DollyAccumulatedPixels = FMath::Clamp(
		DollyAccumulatedPixels + CursorDelta.Y,
		-MaximumAccumulatedPixels,
		MaximumAccumulatedPixels);

	const double ZoomExponent = DollyAccumulatedPixels * DollyZoomRatePerPixel;
	const double NewDistance = FMath::Max(
		DollyInitialDistance * FMath::Exp(ZoomExponent),
		MinimumDollyDistance);
	const FVector DollyDirection = DollyInitialCameraOffset / DollyInitialDistance;

	CapturedViewportClient->SetLookAtLocation(DollyPivot);
	CapturedViewportClient->SetViewRotation(DollyInitialRotation);
	CapturedViewportClient->SetViewLocation(DollyPivot + DollyDirection * NewDistance);
	CapturedViewportClient->Invalidate();
}

FVector FBlendViewNavigationController::ResolveAxisViewCenter() const
{
	if (!CapturedViewportClient)
	{
		return OrbitPivot;
	}

	if (CapturedViewportClient->GetShowWidget() &&
		CapturedViewportClient->GetWidgetMode() != UE::Widget::WM_None)
	{
		return CapturedViewportClient->GetWidgetLocation();
	}

	FVector SelectionPivot = FVector::ZeroVector;
	if (CapturedViewportClient->GetPivotForOrbit(SelectionPivot))
	{
		return SelectionPivot;
	}

	return OrbitPivot;
}

void FBlendViewNavigationController::SnapOrbitToClosestAxis()
{
	if (Mode != EBlendViewNavigationMode::Orbit || !CapturedViewportClient)
	{
		return;
	}

	const FBlendViewAxisSnapCandidate Closest =
		FindClosestAxisSnapCandidate(CapturedViewportClient->GetViewRotation());

	MarkExternalMovement();
	StopNativeMouseTrackingForAxisSnap();
	AxisView = Closest.AxisView;
	OrbitPivot = ResolveAxisViewCenter();
	const FBlendViewAxisSnapViewMatch ViewMatch = CaptureAxisSnapViewMatch(
		CapturedViewportClient,
		CapturedViewport,
		OrbitPivot);
	SetCapturedViewportOrbitCameraEnabled(false);
	CapturedViewportClient->SetViewportType(Closest.ViewportType);
	CapturedViewportClient->SetViewLocation(OrbitPivot);
	CapturedViewportClient->SetLookAtLocation(OrbitPivot, false);
	ApplyAxisSnapViewMatch(
		CapturedViewportClient,
		CapturedViewport,
		OrbitPivot,
		ViewMatch);
	CapturedViewportClient->Invalidate();
}

void FBlendViewNavigationController::MarkExternalMovement() const
{
	if (CapturedViewportClient)
	{
		if (FMouseDeltaTracker* MouseDeltaTracker = CapturedViewportClient->GetMouseDeltaTracker())
		{
			MouseDeltaTracker->SetExternalMovement();
		}
	}
}

void FBlendViewNavigationController::StopNativeMouseTrackingForAxisSnap() const
{
	if (!CapturedViewportClient)
	{
		return;
	}

	if (FMouseDeltaTracker* MouseDeltaTracker = CapturedViewportClient->GetMouseDeltaTracker())
	{
		MouseDeltaTracker->EndTracking(CapturedViewportClient);
		MouseDeltaTracker->ResetUsedDragModifier();
		MouseDeltaTracker->SetExternalMovement();
	}
}
