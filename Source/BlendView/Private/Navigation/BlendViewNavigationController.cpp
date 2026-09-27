// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Navigation/BlendViewNavigationController.h"

#include "BlendViewSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Compat/BlendViewViewportCompat.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "LevelEditorViewport.h"
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
	constexpr double AxisViewWheelZoomExponent = -0.18;
	constexpr double AxisSnapAngleLimitDegrees = 15.0;

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
		float OrthoWidth = 0.0f;
		float ViewDepth = DefaultReferenceDistance;
		float ViewportWidth = 0.0f;
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
		if (!ViewportClient || !Viewport)
		{
			return Match;
		}

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			Viewport,
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags));
		const FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		if (!View)
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

		const FVector ViewLocation = View->ViewLocation;
		const FVector ViewForward = View->GetViewDirection().GetSafeNormal();
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
		const float ProjectionDepthScale = View->IsPerspectiveProjection()
			? PivotDepth
			: 1.0f;
		const float UnitsPerPixelX =
			2.0f * ProjectionDepthScale / (ViewportWidth * ProjectionX);
		const float UnitsPerPixelY =
			2.0f * ProjectionDepthScale / (ViewportHeight * ProjectionY);
		const float UnitsPerPixel = (UnitsPerPixelX + UnitsPerPixelY) * 0.5f;
		if (UnitsPerPixel <= UE_SMALL_NUMBER)
		{
			return Match;
		}

		Match.bValid = true;
		Match.PivotPixelOffsetFromCenter = PivotPixel - ViewCenter;
		Match.OrthoZoom = UnitsPerPixel * ViewportWidth * 15.0f / GetEditorOrthoZoomFactor(ViewportWidth);
		Match.OrthoWidth = UnitsPerPixel * ViewportWidth;
		Match.ViewDepth = PivotDepth;
		Match.ViewportWidth = ViewportWidth;
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
		FVector Forward = FVector::ForwardVector;
		FVector Up = FVector::UpVector;
		FRotator Rotation = FRotator::ZeroRotator;
		int32 RollQuarter = 0;
		double Score = -TNumericLimits<double>::Max();
	};

	FBlendViewAxisSnapCandidate FindClosestAxisSnapCandidate(
		const FVector& CurrentForward,
		const FVector& CurrentUp)
	{
		struct FAxisDefinition
		{
			EBlendViewAxisView AxisView;
			ELevelViewportType ViewportType;
			FVector Forward;
			FVector BaseUp;
		};

		static const FAxisDefinition AxisDefinitions[] =
		{
			{EBlendViewAxisView::Front, LVT_OrthoFront, FVector::BackwardVector, FVector::UpVector},
			{EBlendViewAxisView::Back, LVT_OrthoBack, FVector::ForwardVector, FVector::UpVector},
			{EBlendViewAxisView::Left, LVT_OrthoLeft, FVector::LeftVector, FVector::UpVector},
			{EBlendViewAxisView::Right, LVT_OrthoRight, FVector::RightVector, FVector::UpVector},
			{EBlendViewAxisView::Top, LVT_OrthoTop, FVector::DownVector, FVector::ForwardVector},
			{EBlendViewAxisView::Bottom, LVT_OrthoBottom, FVector::UpVector, FVector::ForwardVector}
		};

		const FVector NormalizedForward = CurrentForward.GetSafeNormal();
		const FVector NormalizedUp = CurrentUp.GetSafeNormal();
		const FQuat CurrentOrientation =
			FRotationMatrix::MakeFromXZ(NormalizedForward, NormalizedUp).ToQuat();
		const FAxisDefinition* ClosestAxis = nullptr;
		double ClosestForwardScore = -TNumericLimits<double>::Max();
		for (const FAxisDefinition& Axis : AxisDefinitions)
		{
			const double ForwardScore = FVector::DotProduct(
				NormalizedForward,
				Axis.Forward.GetSafeNormal());
			if (ForwardScore > ClosestForwardScore)
			{
				ClosestForwardScore = ForwardScore;
				ClosestAxis = &Axis;
			}
		}

		FBlendViewAxisSnapCandidate Best;
		const double MinimumForwardScore = FMath::Cos(
			FMath::DegreesToRadians(AxisSnapAngleLimitDegrees));
		if (!ClosestAxis || ClosestForwardScore < MinimumForwardScore)
		{
			return Best;
		}

		const FVector AxisForward = ClosestAxis->Forward.GetSafeNormal();
		for (int32 RollQuarter = 0; RollQuarter < 4; ++RollQuarter)
		{
			const FQuat RollRotation(
				AxisForward,
				FMath::DegreesToRadians(90.0 * static_cast<double>(RollQuarter)));
			const FVector CandidateUp =
				RollRotation.RotateVector(ClosestAxis->BaseUp).GetSafeNormal();
			const FQuat CandidateOrientation =
				FRotationMatrix::MakeFromXZ(AxisForward, CandidateUp).ToQuat();
			const double Score = FMath::Abs(
				CurrentOrientation.X * CandidateOrientation.X +
				CurrentOrientation.Y * CandidateOrientation.Y +
				CurrentOrientation.Z * CandidateOrientation.Z +
				CurrentOrientation.W * CandidateOrientation.W);
			if (Score > Best.Score)
			{
				Best.AxisView = ClosestAxis->AxisView;
				Best.ViewportType = ClosestAxis->ViewportType;
				Best.Forward = AxisForward;
				Best.Up = CandidateUp;
				Best.Rotation = CandidateOrientation.Rotator();
				Best.RollQuarter = RollQuarter;
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
	if (ViewportContext.Kind == EBlendViewViewportKind::LevelEditor)
	{
		FLevelEditorViewportClient* LevelViewportClient =
			static_cast<FLevelEditorViewportClient*>(ViewportContext.ViewportClient);
		if (AxisViewCamera.IsValid() &&
			!IsAxisViewCameraActive(ViewportContext.ViewportClient))
		{
			ReleaseAxisViewCamera(true);
		}
		if (LevelViewportClient->IsAnyActorLocked() &&
			!IsAxisViewCameraActive(ViewportContext.ViewportClient))
		{
			return false;
		}
	}

	CapturedViewportLifetimeGuard = ViewportContext.ViewportWidget.Pin();
	if (!CapturedViewportLifetimeGuard.IsValid())
	{
		return false;
	}
	CapturedViewportClient = ViewportContext.ViewportClient;
	CapturedViewport = ViewportContext.Viewport;
	bCapturedLevelEditorViewport = ViewportContext.Kind == EBlendViewViewportKind::LevelEditor;
	bPersistentOrbitViewport = CapturedViewportClient->ShouldOrbitCamera();
	bCapturedInitialOrbitCamera = CapturedViewportClient->bUsingOrbitCamera;
	CaptureInitialViewState();
	const bool bBeginPan = Event.bShiftDown && !Event.bControlDown;
	const bool bBeginDolly = Event.bControlDown;
	const bool bAxisCameraActive = IsAxisViewCameraActive(CapturedViewportClient);
	if (bAxisCameraActive && !bBeginPan && !bBeginDolly)
	{
		DetachAxisViewCameraForOrbit();
	}
	else if (!bAxisCameraActive && !bBeginPan && !CapturedViewportClient->IsPerspective())
	{
		CapturedViewportClient->SetViewportType(LVT_Perspective);
	}
	bUsingOrbitCamera = CapturedViewportClient->bUsingOrbitCamera;
	bAxisSnapModifierActive = Event.bAltDown;

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
	if (Mode == EBlendViewNavigationMode::Orbit && bAxisSnapModifierActive)
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
		bAxisSnapModifierActive = true;
		SnapOrbitToClosestAxis();
		return EBlendViewInputResult::Handled;
	}
	if (Mode == EBlendViewNavigationMode::Orbit &&
		Event.Type == EBlendViewInputEventType::KeyUp &&
		(Event.Key == EKeys::LeftAlt || Event.Key == EKeys::RightAlt))
	{
		bAxisSnapModifierActive = Event.bAltDown;
		if (!bAxisSnapModifierActive)
		{
			ReleaseAxisSnapToWorkingView();
		}
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
		switch (Mode)
		{
		case EBlendViewNavigationMode::Orbit:
			UpdateOrbit(Event.CursorDelta, !bAxisSnapModifierActive);
			if (bAxisSnapModifierActive)
			{
				SnapOrbitToClosestAxis();
			}
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

EBlendViewInputResult FBlendViewNavigationController::RoutePersistentAxisViewInput(
	const FBlendViewInputEvent& Event,
	const FBlendViewViewportContext& ViewportContext)
{
	if (Event.Type != EBlendViewInputEventType::MouseWheel ||
		FMath::IsNearlyZero(Event.WheelDelta) ||
		!ViewportContext.IsValid() ||
		!IsAxisViewCameraActive(ViewportContext.ViewportClient))
	{
		return EBlendViewInputResult::PassThrough;
	}

	ACameraActor* CameraActor = AxisViewCamera.Get();
	UCameraComponent* CameraComponent = CameraActor ? CameraActor->GetCameraComponent() : nullptr;
	if (!CameraComponent)
	{
		return EBlendViewInputResult::PassThrough;
	}

	const double ZoomScale = FMath::Exp(
		static_cast<double>(Event.WheelDelta) * AxisViewWheelZoomExponent);
	CameraComponent->SetOrthoWidth(FMath::Clamp<float>(
		CameraComponent->OrthoWidth * ZoomScale,
		1.0f,
		UE_LARGE_HALF_WORLD_MAX));
	if (AxisViewViewportClient)
	{
		AxisViewViewportClient->UpdateViewForLockedActor();
		AxisViewViewportClient->Invalidate();
	}
	return EBlendViewInputResult::Handled;
}

void FBlendViewNavigationController::EndNavigation()
{
	if (CapturedViewportClient)
	{
		const bool bAxisCameraActive = IsAxisViewCameraActive(CapturedViewportClient);
		SetCapturedViewportOrbitCameraEnabled(bAxisCameraActive ? false : bCapturedInitialOrbitCamera);
		if (!bAxisCameraActive && AxisViewCamera.IsValid())
		{
			ReleaseAxisViewCamera(true);
		}
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
	DollyInitialOrthoWidth = 0.0f;
	bUsingOrbitCamera = false;
	bPersistentOrbitViewport = false;
	bCapturedLevelEditorViewport = false;
	bCapturedInitialOrbitCamera = false;
	bHasCapturedInitialViewState = false;
	bCapturedInitialAxisCameraActive = false;
	bHasOrbitWorkingView = false;
	bAxisSnapModifierActive = false;
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
	bCapturedInitialAxisCameraActive = IsAxisViewCameraActive(CapturedViewportClient);
	if (bCapturedInitialAxisCameraActive)
	{
		CapturedInitialAxisViewPivot = PersistentAxisViewPivot;
		if (const ACameraActor* CameraActor = AxisViewCamera.Get())
		{
			CapturedInitialAxisCameraTransform = CameraActor->GetActorTransform();
			if (const UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
			{
				CapturedInitialAxisCameraOrthoWidth = CameraComponent->OrthoWidth;
			}
		}
	}
	bHasCapturedInitialViewState = true;
}

void FBlendViewNavigationController::RestoreInitialViewState()
{
	if (!CapturedViewportClient || !bHasCapturedInitialViewState)
	{
		return;
	}
	if (bCapturedInitialAxisCameraActive && AxisViewCamera.IsValid())
	{
		PersistentAxisViewPivot = CapturedInitialAxisViewPivot;
		ACameraActor* CameraActor = AxisViewCamera.Get();
		CameraActor->SetActorTransform(CapturedInitialAxisCameraTransform);
		if (UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
		{
			CameraComponent->SetOrthoWidth(CapturedInitialAxisCameraOrthoWidth);
		}
		if (FLevelEditorViewportClient* LevelViewportClient = GetCapturedLevelViewportClient())
		{
			LevelViewportClient->SetViewportType(LVT_Perspective);
			LevelViewportClient->SetActorLock(CameraActor);
			LevelViewportClient->bLockedCameraView = true;
			LevelViewportClient->UpdateViewForLockedActor();
			LevelViewportClient->Invalidate();
		}
		return;
	}
	ReleaseAxisViewCamera(true);

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
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (Settings && Settings->bOrbitAroundSelection)
	{
		FVector SelectionCenter = FVector::ZeroVector;
		if (TryResolveOrbitSelectionCenter(SelectionCenter))
		{
			OrbitPivot = SelectionCenter;
		}
	}
	OrbitWorkingLocation = CapturedViewportClient->GetViewLocation();
	OrbitWorkingRotation = CapturedViewportClient->GetViewRotation();
	bHasOrbitWorkingView = true;

	const FVector CameraLocation = OrbitWorkingLocation;
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
	if (IsAxisViewCameraActive(CapturedViewportClient))
	{
		ACameraActor* CameraActor = AxisViewCamera.Get();
		UCameraComponent* CameraComponent = CameraActor
			? CameraActor->GetCameraComponent()
			: nullptr;
		if (!CameraActor || !CameraComponent)
		{
			return;
		}

		PanCameraRight = CameraActor->GetActorRightVector();
		PanCameraUp = CameraActor->GetActorUpVector();
		float ViewportWidth = static_cast<float>(CapturedViewport->GetSizeXY().X);
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			CapturedViewport,
			CapturedViewportClient->GetScene(),
			CapturedViewportClient->EngineShowFlags));
		if (const FSceneView* View = CapturedViewportClient->CalcSceneView(&ViewFamily))
		{
			PanCameraRight = View->GetViewRight().GetSafeNormal();
			PanCameraUp = View->GetViewUp().GetSafeNormal();
			ViewportWidth = static_cast<float>(View->UnscaledViewRect.Width());
		}
		const float UnitsPerPixel = CameraComponent->OrthoWidth /
			FMath::Max(ViewportWidth, 1.0f);
		PanWorldUnitsPerPixel = FVector2D(UnitsPerPixel, UnitsPerPixel);
		return;
	}

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
	DollyInitialOrthoWidth = 0.0f;
	if (IsAxisViewCameraActive(CapturedViewportClient))
	{
		if (const ACameraActor* CameraActor = AxisViewCamera.Get())
		{
			if (const UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
			{
				DollyInitialOrthoWidth = CameraComponent->OrthoWidth;
			}
		}
		return;
	}

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
		if (AxisView != EBlendViewAxisView::None)
		{
			ReleaseAxisSnapToWorkingView();
		}
		bAxisSnapModifierActive = false;
		InitializePan(MakeCapturedViewportContext(Event));
		break;
	case EBlendViewNavigationMode::Dolly:
		if (AxisView != EBlendViewAxisView::None)
		{
			ReleaseAxisSnapToWorkingView();
		}
		bAxisSnapModifierActive = false;
		InitializeDolly();
		break;
	case EBlendViewNavigationMode::Orbit:
		InitializeOrbit();
		bAxisSnapModifierActive = Event.bAltDown;
		if (bAxisSnapModifierActive)
		{
			SnapOrbitToClosestAxis();
		}
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

void FBlendViewNavigationController::UpdateOrbit(
	const FVector2D& CursorDelta,
	const bool bApplyToViewport)
{
	if (!CapturedViewportClient || !bHasOrbitWorkingView)
	{
		return;
	}

	MarkExternalMovement();

	const FVector CameraLocation = OrbitWorkingLocation;
	const FRotator CameraRotation = OrbitWorkingRotation;

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

		const float OrbitDistance = FMath::Max(
			FVector::Distance(CameraLocation, OrbitPivot),
			MinimumReferenceDistance);
		OrbitWorkingRotation = OrbitRotation;
		OrbitWorkingLocation = OrbitPivot - OrbitRotation.Vector() * OrbitDistance;
		if (bApplyToViewport)
		{
			ApplyOrbitWorkingView();
		}
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
	OrbitWorkingLocation = NewLocation;
	OrbitWorkingRotation = NewRotation;
	if (bApplyToViewport)
	{
		ApplyOrbitWorkingView();
	}
}

void FBlendViewNavigationController::ApplyOrbitWorkingView()
{
	if (!CapturedViewportClient || !bHasOrbitWorkingView)
	{
		return;
	}

	CapturedViewportClient->SetViewportType(LVT_Perspective);
	CapturedViewportClient->SetViewLocation(OrbitWorkingLocation);
	CapturedViewportClient->SetViewRotation(OrbitWorkingRotation);
	CapturedViewportClient->SetLookAtLocation(OrbitPivot, false);
	CapturedViewportClient->Invalidate();
}

void FBlendViewNavigationController::ReleaseAxisSnapToWorkingView()
{
	if (!CapturedViewportClient || AxisView == EBlendViewAxisView::None)
	{
		return;
	}

	ReleaseAxisViewCamera(false);
	AxisView = EBlendViewAxisView::None;
	ApplyOrbitWorkingView();
}

void FBlendViewNavigationController::UpdatePan(const FVector2D& CursorDelta)
{
	if (!CapturedViewportClient)
	{
		return;
	}

	MarkExternalMovement();

	const FVector PanDelta =
		-PanCameraRight * CursorDelta.X * PanWorldUnitsPerPixel.X +
		PanCameraUp * CursorDelta.Y * PanWorldUnitsPerPixel.Y;
	if (IsAxisViewCameraActive(CapturedViewportClient))
	{
		if (ACameraActor* CameraActor = AxisViewCamera.Get())
		{
			CameraActor->AddActorWorldOffset(PanDelta);
			PersistentAxisViewPivot += PanDelta;
			if (FLevelEditorViewportClient* LevelViewportClient = GetCapturedLevelViewportClient())
			{
				LevelViewportClient->UpdateViewForLockedActor();
				LevelViewportClient->Invalidate();
			}
		}
		return;
	}

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
	if (!CapturedViewportClient)
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
	if (IsAxisViewCameraActive(CapturedViewportClient) && DollyInitialOrthoWidth > 0.0f)
	{
		if (ACameraActor* CameraActor = AxisViewCamera.Get())
		{
			if (UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
			{
				CameraComponent->SetOrthoWidth(FMath::Clamp<float>(
					DollyInitialOrthoWidth * FMath::Exp(ZoomExponent),
					1.0f,
					UE_LARGE_HALF_WORLD_MAX));
				if (FLevelEditorViewportClient* LevelViewportClient = GetCapturedLevelViewportClient())
				{
					LevelViewportClient->UpdateViewForLockedActor();
					LevelViewportClient->Invalidate();
				}
			}
		}
		return;
	}
	if (DollyInitialDistance < MinimumDollyDistance)
	{
		return;
	}
	const double NewDistance = FMath::Max(
		DollyInitialDistance * FMath::Exp(ZoomExponent),
		MinimumDollyDistance);
	const FVector DollyDirection = DollyInitialCameraOffset / DollyInitialDistance;

	CapturedViewportClient->SetLookAtLocation(DollyPivot);
	CapturedViewportClient->SetViewRotation(DollyInitialRotation);
	CapturedViewportClient->SetViewLocation(DollyPivot + DollyDirection * NewDistance);
	CapturedViewportClient->Invalidate();
}

bool FBlendViewNavigationController::TryResolveOrbitSelectionCenter(FVector& OutCenter) const
{
	if (!CapturedViewportClient)
	{
		return false;
	}

	return CapturedViewportClient->GetPivotForOrbit(OutCenter);
}

void FBlendViewNavigationController::SnapOrbitToClosestAxis()
{
	if (Mode != EBlendViewNavigationMode::Orbit || !CapturedViewportClient)
	{
		return;
	}

	const FRotator SourceRotation = bHasOrbitWorkingView
		? OrbitWorkingRotation
		: CapturedViewportClient->GetViewRotation();
	const FRotationMatrix RotationMatrix(SourceRotation);
	FVector ViewForward = RotationMatrix.GetUnitAxis(EAxis::X).GetSafeNormal();
	FVector ViewUp = RotationMatrix.GetUnitAxis(EAxis::Z).GetSafeNormal();
	if (CapturedViewport && !bHasOrbitWorkingView)
	{
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			CapturedViewport,
			CapturedViewportClient->GetScene(),
			CapturedViewportClient->EngineShowFlags));
		if (const FSceneView* View = CapturedViewportClient->CalcSceneView(&ViewFamily))
		{
			ViewForward = View->GetViewDirection().GetSafeNormal();
			ViewUp = View->GetViewUp().GetSafeNormal();
		}
	}
	const FBlendViewAxisSnapCandidate Closest =
		FindClosestAxisSnapCandidate(ViewForward, ViewUp);

	MarkExternalMovement();
	if (Closest.AxisView == EBlendViewAxisView::None)
	{
		if (AxisView != EBlendViewAxisView::None)
		{
			ReleaseAxisSnapToWorkingView();
		}
		else
		{
			ApplyOrbitWorkingView();
		}
		return;
	}

	const bool bEnteringAxisSnap = AxisView == EBlendViewAxisView::None;
	if (bEnteringAxisSnap)
	{
		StopNativeMouseTrackingForAxisSnap();
	}
	AxisView = Closest.AxisView;
	const FBlendViewAxisSnapViewMatch ViewMatch = CaptureAxisSnapViewMatch(
		CapturedViewportClient,
		CapturedViewport,
		OrbitPivot);
	float TargetOrthoWidth = ViewMatch.OrthoWidth;
	float TargetDepth = ViewMatch.ViewDepth;
	float TargetViewportWidth = ViewMatch.ViewportWidth;
	FVector2D TargetPixelOffset = ViewMatch.PivotPixelOffsetFromCenter;
	if (!ViewMatch.bValid)
	{
		TargetViewportWidth = CapturedViewport
			? static_cast<float>(CapturedViewport->GetSizeXY().X)
			: 1.0f;
		TargetDepth = GetReferenceDistance(CapturedViewportClient);
		const float HalfHorizontalFov = FMath::DegreesToRadians(
			FMath::Clamp(CapturedViewportClient->ViewFOV, 1.0f, 179.0f) * 0.5f);
		TargetOrthoWidth = 2.0f * TargetDepth * FMath::Tan(HalfHorizontalFov);
		TargetPixelOffset = FVector2D::ZeroVector;
	}

	const FRotationMatrix TargetRotationMatrix(Closest.Rotation);
	const FVector TargetRight = TargetRotationMatrix.GetUnitAxis(EAxis::Y);
	const FVector TargetUp = TargetRotationMatrix.GetUnitAxis(EAxis::Z);
	const float UnitsPerPixel = TargetOrthoWidth / FMath::Max(TargetViewportWidth, 1.0f);
	const FVector TargetCenter =
		OrbitPivot -
		TargetRight * TargetPixelOffset.X * UnitsPerPixel +
		TargetUp * TargetPixelOffset.Y * UnitsPerPixel;
	const FVector TargetLocation =
		TargetCenter - Closest.Forward * FMath::Max(TargetDepth, MinimumReferenceDistance);
	if (ActivateAxisViewCamera(TargetLocation, Closest.Rotation, TargetOrthoWidth))
	{
		return;
	}

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

bool FBlendViewNavigationController::ActivateAxisViewCamera(
	const FVector& Location,
	const FRotator& Rotation,
	const float OrthoWidth)
{
	FLevelEditorViewportClient* LevelViewportClient = GetCapturedLevelViewportClient();
	if (!LevelViewportClient || !CapturedViewportLifetimeGuard.IsValid())
	{
		return false;
	}

	ACameraActor* CameraActor = AxisViewCamera.Get();
	if (LevelViewportClient->IsAnyActorLocked() &&
		(!CameraActor || !LevelViewportClient->IsActorLocked(CameraActor)))
	{
		return false;
	}

	if (!CameraActor)
	{
		UWorld* World = LevelViewportClient->GetWorld();
		if (!World)
		{
			return false;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient | RF_DuplicateTransient | RF_TextExportTransient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
#if WITH_EDITOR
		SpawnParameters.bTemporaryEditorActor = true;
		SpawnParameters.bHideFromSceneOutliner = true;
#endif
		CameraActor = World->SpawnActor<ACameraActor>(Location, Rotation, SpawnParameters);
		if (!CameraActor)
		{
			return false;
		}
		CameraActor->SetIsTemporarilyHiddenInEditor(true);
		AxisViewCamera = CameraActor;
	}

	UCameraComponent* CameraComponent = CameraActor->GetCameraComponent();
	if (!CameraComponent)
	{
		ReleaseAxisViewCamera(true);
		return false;
	}

	CameraActor->SetActorLocationAndRotation(Location, Rotation);
	CameraComponent->SetProjectionMode(ECameraProjectionMode::Orthographic);
	CameraComponent->SetOrthoWidth(FMath::Clamp(
		OrthoWidth,
		1.0f,
		static_cast<float>(UE_LARGE_HALF_WORLD_MAX)));
	CameraComponent->SetConstraintAspectRatio(false);
	CameraComponent->SetAutoCalculateOrthoPlanes(true);
	CameraComponent->SetUpdateOrthoPlanes(true);
	CameraComponent->SetUseCameraHeightAsViewTarget(false);

	SetCapturedViewportOrbitCameraEnabled(false);
	PersistentAxisViewPivot = OrbitPivot;
	AxisViewViewportClient = LevelViewportClient;
	AxisViewViewportLifetimeGuard = CapturedViewportLifetimeGuard;
	LevelViewportClient->SetViewportType(LVT_Perspective);
	LevelViewportClient->SetActorLock(CameraActor);
	LevelViewportClient->bLockedCameraView = true;
	LevelViewportClient->UpdateViewForLockedActor();
	LevelViewportClient->SetLookAtLocation(PersistentAxisViewPivot, false);
	LevelViewportClient->Invalidate();
	return true;
}

void FBlendViewNavigationController::DetachAxisViewCameraForOrbit()
{
	if (!CapturedViewportClient || !IsAxisViewCameraActive(CapturedViewportClient))
	{
		return;
	}

	ACameraActor* CameraActor = AxisViewCamera.Get();
	UCameraComponent* CameraComponent = CameraActor
		? CameraActor->GetCameraComponent()
		: nullptr;
	if (!CameraActor || !CameraComponent)
	{
		ReleaseAxisViewCamera(true);
		return;
	}

	const FVector AxisCameraLocation = CameraActor->GetActorLocation();
	const FRotator AxisCameraRotation = CameraActor->GetActorRotation();
	const FVector ViewForward = AxisCameraRotation.Vector();
	float ViewDepth = FVector::DotProduct(
		PersistentAxisViewPivot - AxisCameraLocation,
		ViewForward);
	if (ViewDepth < MinimumReferenceDistance)
	{
		ViewDepth = DefaultReferenceDistance;
	}
	const FVector ViewCenter = AxisCameraLocation + ViewForward * ViewDepth;
	const float HalfHorizontalFov = FMath::DegreesToRadians(
		FMath::Clamp(CapturedViewportClient->ViewFOV, 1.0f, 179.0f) * 0.5f);
	const float PerspectiveDepth = FMath::Max(
		CameraComponent->OrthoWidth / (2.0f * FMath::Tan(HalfHorizontalFov)),
		MinimumReferenceDistance);

	ReleaseAxisViewCamera(false);
	CapturedViewportClient->SetViewportType(LVT_Perspective);
	CapturedViewportClient->SetViewRotation(AxisCameraRotation);
	CapturedViewportClient->SetViewLocation(ViewCenter - ViewForward * PerspectiveDepth);
	CapturedViewportClient->SetLookAtLocation(PersistentAxisViewPivot, false);
	OrbitPivot = PersistentAxisViewPivot;
	CapturedViewportClient->Invalidate();
}

void FBlendViewNavigationController::ReleaseAxisViewCamera(const bool bDestroyActor)
{
	ACameraActor* CameraActor = AxisViewCamera.Get();
	if (AxisViewViewportClient &&
		AxisViewViewportLifetimeGuard.IsValid() &&
		CameraActor &&
		AxisViewViewportClient->IsActorLocked(CameraActor))
	{
		AxisViewViewportClient->SetActorLock(static_cast<AActor*>(nullptr));
		AxisViewViewportClient->bLockedCameraView = false;
		AxisViewViewportClient->UpdateViewForLockedActor();
		AxisViewViewportClient->Invalidate();
	}

	if (!bDestroyActor)
	{
		return;
	}

	if (CameraActor && CameraActor->GetWorld())
	{
		CameraActor->Destroy();
	}
	AxisViewCamera.Reset();
	AxisViewViewportClient = nullptr;
	AxisViewViewportLifetimeGuard.Reset();
	PersistentAxisViewPivot = FVector::ZeroVector;
}

bool FBlendViewNavigationController::IsAxisViewCameraActive(
	const FEditorViewportClient* ViewportClient) const
{
	const ACameraActor* CameraActor = AxisViewCamera.Get();
	return CameraActor &&
		AxisViewViewportClient &&
		AxisViewViewportLifetimeGuard.IsValid() &&
		ViewportClient == AxisViewViewportClient &&
		AxisViewViewportClient->bLockedCameraView &&
		AxisViewViewportClient->IsActorLocked(CameraActor);
}

FLevelEditorViewportClient* FBlendViewNavigationController::GetCapturedLevelViewportClient() const
{
	return bCapturedLevelEditorViewport && CapturedViewportClient
		? static_cast<FLevelEditorViewportClient*>(CapturedViewportClient)
		: nullptr;
}

void FBlendViewNavigationController::ResetPersistentAxisView()
{
	if (IsNavigating())
	{
		CancelNavigation();
	}
	ReleaseAxisViewCamera(true);
}

void FBlendViewNavigationController::PrepareForEngineExit()
{
	Mode = EBlendViewNavigationMode::None;
	AxisView = EBlendViewAxisView::None;
	CapturedViewportClient = nullptr;
	CapturedViewport = nullptr;
	CapturedViewportLifetimeGuard.Reset();
	AxisViewCamera.Reset();
	AxisViewViewportClient = nullptr;
	AxisViewViewportLifetimeGuard.Reset();
	bCapturedLevelEditorViewport = false;
	bHasCapturedInitialViewState = false;
	bCapturedInitialAxisCameraActive = false;
	bHasOrbitWorkingView = false;
	bAxisSnapModifierActive = false;
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
