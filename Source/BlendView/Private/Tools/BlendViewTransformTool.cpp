// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewTransformTool.h"

#include "BlendViewCommands.h"
#include "BlendViewSettings.h"
#include "Compat/BlendViewViewportCompat.h"
#include "Core/BlendViewSessionState.h"
#include "Cursor/BlendViewSceneCursorState.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "LevelEditor.h"
#include "Misc/AxisDisplayInfo.h"
#include "Modules/ModuleManager.h"
#include "Slate/SceneViewport.h"
#include "SLevelViewport.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "StaticMeshResources.h"
#include "UI/BlendViewStatusBarPresenter.h"
#include "UI/BlendViewTransformCanvasDrawing.h"
#include "UI/BlendViewTransformOverlay.h"
#include "UI/BlendViewTransformValueFormatter.h"
#include "UI/BlendViewWorldPivotVisualizer.h"
#include "Localization/BlendViewLocalization.h"
#include "Tools/BlendViewTransformMath.h"
#include "Tools/BlendViewTransformConstraintUtils.h"
#include "Tools/BlendViewTransformPrecision.h"
#include "Tools/BlendViewTransformResetPolicy.h"
#include "Tools/BlendViewTransformStatusBuilder.h"
#include "Tools/BlendViewTransformSnapPolicy.h"
#include "Viewport/BlendViewViewportContext.h"
#include "Viewport/BlendViewViewportGeometry.h"
#include "Viewport/BlendViewViewportProjector.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateUser.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlendViewTransform, Log, All);

namespace
{
	constexpr double PrecisionMouseScale = 0.1;
	constexpr double RotationPrecisionMouseScale = 1.0 / 30.0;
	constexpr double MinAxisDirectionProbeWorldDistance = 1000.0;

	FVector GetActorSnapshotPivotWorldLocation(
		const FTransform& Transform,
		const FVector& PivotOffset)
	{
		return Transform.TransformPosition(PivotOffset);
	}

	bool TryResolveActiveActorPivot(
		const FBlendViewTransformTargetAdapter& TargetAdapter,
		FVector& OutPivotLocation,
		FQuat& OutLocalConstraintRotation)
	{
		const AActor* ActiveActor = FBlendViewSessionState::GetActiveActor();
		if (!IsValid(ActiveActor))
		{
			return false;
		}

		for (const FBlendViewActorTransformSnapshot& Snapshot : TargetAdapter.GetActorSnapshots())
		{
			if (Snapshot.Actor.Get() == ActiveActor)
			{
				OutPivotLocation = GetActorSnapshotPivotWorldLocation(Snapshot.InitialTransform, Snapshot.InitialPivotOffset);
				OutLocalConstraintRotation = Snapshot.InitialTransform.GetRotation().GetNormalized();
				return true;
			}
		}
		return false;
	}

	bool IsControlModifierKey(const FKey& Key)
	{
		return Key == EKeys::LeftControl || Key == EKeys::RightControl;
	}

	FLinearColor GetBlendViewAxisColor(const EBlendViewAxisConstraint Axis, const bool bIsActive)
	{
		FLinearColor Color = FLinearColor::White;
		switch (Axis)
		{
		case EBlendViewAxisConstraint::X:
			Color = AxisDisplayInfo::GetAxisColor(EAxisList::X);
			break;
		case EBlendViewAxisConstraint::Y:
			Color = AxisDisplayInfo::GetAxisColor(EAxisList::Y);
			break;
		case EBlendViewAxisConstraint::Z:
			Color = AxisDisplayInfo::GetAxisColor(EAxisList::Z);
			break;
		default:
			break;
		}

		if (bIsActive)
		{
			Color.R = FMath::Min(Color.R * 1.25f, 1.0f);
			Color.G = FMath::Min(Color.G * 1.25f, 1.0f);
			Color.B = FMath::Min(Color.B * 1.25f, 1.0f);
			Color.A = 1.0f;
		}
		else
		{
			Color.R *= 0.32f;
			Color.G *= 0.32f;
			Color.B *= 0.32f;
			Color.A = 1.0f;
		}
		return Color;
	}

	TOptional<EBlendViewTransformMode> GetRequestedTransformModeFromCommands(const FBlendViewInputEvent& Event)
	{
		if (!FBlendViewCommands::IsRegistered())
		{
			return TOptional<EBlendViewTransformMode>();
		}

		const FBlendViewCommands& Commands = FBlendViewCommands::Get();
		if (Commands.BeginTranslate.IsValid() &&
			FBlendViewCommands::MatchesInputEvent(Event, *Commands.BeginTranslate))
		{
			return EBlendViewTransformMode::Translate;
		}
		if (Commands.BeginRotate.IsValid() &&
			FBlendViewCommands::MatchesInputEvent(Event, *Commands.BeginRotate))
		{
			return EBlendViewTransformMode::Rotate;
		}
		if (Commands.BeginScale.IsValid() &&
			FBlendViewCommands::MatchesInputEvent(Event, *Commands.BeginScale))
		{
			return EBlendViewTransformMode::Scale;
		}
		if (Commands.BeginMirror.IsValid() &&
			FBlendViewCommands::MatchesInputEvent(Event, *Commands.BeginMirror))
		{
			return EBlendViewTransformMode::Mirror;
		}

		return TOptional<EBlendViewTransformMode>();
	}
}

FBlendViewTransformTool::FBlendViewTransformTool(const EBlendViewTransformMode InMode)
	: Mode(InMode)
{
}

FBlendViewTransformTool::FBlendViewTransformTool(
	const EBlendViewTransformMode InMode,
	TUniquePtr<FScopedTransaction> InTransaction,
	TOptional<FVector> InInitialPivotOverride,
	TArray<TWeakObjectPtr<USceneComponent>> InExplicitComponents)
	: Mode(InMode)
	, PendingTransaction(MoveTemp(InTransaction))
	, ExplicitComponents(MoveTemp(InExplicitComponents))
	, InitialPivotOverride(MoveTemp(InInitialPivotOverride))
{
}

FBlendViewTransformTool::~FBlendViewTransformTool()
{
}

bool FBlendViewTransformTool::WantsInputBeforeModalShortcuts(const FBlendViewInputEvent& Event) const
{
	if ((Event.Type == EBlendViewInputEventType::MouseDown ||
			Event.Type == EBlendViewInputEventType::MouseUp) &&
		Event.Key == EKeys::MiddleMouseButton)
	{
		return true;
	}

	if (!SnapSession.bSelectingBase)
	{
		return false;
	}

	return (Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::LeftMouseButton) ||
		Event.Type == EBlendViewInputEventType::MouseMove;
}

bool FBlendViewTransformTool::Begin(const FBlendViewViewportContext& Context)
{
	if (!Context.IsValid())
	{
		return false;
	}

	ViewportLifetimeGuard = Context.ViewportWidget.Pin();
	if (!ViewportLifetimeGuard.IsValid())
	{
		return false;
	}
	InitialContext = Context;
	bCompleteRequested = false;
	if (!CaptureSelection(Context))
	{
		ViewportLifetimeGuard.Reset();
		UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform ignored: no transformable selection"));
		return false;
	}
	ResetAppliedTransformValues();
	AttachViewportOverlay();
	ApplyTransformCursorOverride();
	UpdateViewportOverlay();
	UpdateWorldPivotVisualization();

	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform begin: %s Actors=%d Pivot=%s"),
		GetModeName(),
		TargetAdapter.GetActorSnapshots().Num(),
		*InitialPivotLocation.ToString());
	return true;
}

EBlendViewInputResult FBlendViewTransformTool::HandleInput(const FBlendViewInputEvent& Event)
{
	if (FBlendViewCommands::IsRegistered() &&
		FBlendViewCommands::Get().ToggleTransformStatusBar.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewCommands::Get().ToggleTransformStatusBar))
	{
		FBlendViewSessionState::ToggleStatusHints();
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
		return EBlendViewInputResult::Handled;
	}

	if (SnapSession.bSelectingBase)
	{
		if (Event.Type == EBlendViewInputEventType::MouseMove)
		{
			UpdateSnapBaseCandidateFromEvent(Event);
			return EBlendViewInputResult::Handled;
		}
		if (Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::LeftMouseButton)
		{
			UpdateSnapBaseCandidateFromEvent(Event);
			ConfirmSnapBaseSelection();
			return EBlendViewInputResult::Handled;
		}
		if ((Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::RightMouseButton) ||
			(Event.Type == EBlendViewInputEventType::KeyDown && Event.Key == EKeys::Escape))
		{
			CancelSnapBaseSelection();
			return EBlendViewInputResult::Handled;
		}
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::MiddleMouseButton)
	{
		BeginMiddleMouseAxisSelection(Event);
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::MouseUp && Event.Key == EKeys::MiddleMouseButton)
	{
		EndMiddleMouseAxisSelection(Event);
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::MouseMove)
	{
		if (NumericInput.IsActive())
		{
			return EBlendViewInputResult::Handled;
		}

		switch (Mode)
		{
		case EBlendViewTransformMode::Translate:
			UpdateTranslation(Event);
			break;
		case EBlendViewTransformMode::Rotate:
			UpdateRotation(Event);
			break;
		case EBlendViewTransformMode::Scale:
			UpdateScale(Event);
			break;
		case EBlendViewTransformMode::Mirror:
			UpdateMirror(Event);
			break;
		default:
			break;
		}
		UpdateViewportOverlay();
		return EBlendViewInputResult::Handled;
	}

	if ((Event.Type == EBlendViewInputEventType::KeyDown || Event.Type == EBlendViewInputEventType::KeyUp) &&
		IsControlModifierKey(Event.Key))
	{
		const bool bSnapActive = Event.Type == EBlendViewInputEventType::KeyDown && !NumericInput.IsActive();
		if (Mode == EBlendViewTransformMode::Translate)
		{
			RefreshTranslationFromCurrentIntent(ShouldUseTranslationSnap(bSnapActive));
		}
		else if (Mode == EBlendViewTransformMode::Rotate)
		{
			RefreshRotationFromCurrentIntent(ShouldUseIncrementSnap(bSnapActive));
		}
		else if (Mode == EBlendViewTransformMode::Scale)
		{
			RefreshScaleFromCurrentIntent(ShouldUseIncrementSnap(bSnapActive), Event.bShiftDown);
		}
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::KeyDown && Event.bIsRepeat)
	{
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::KeyDown &&
		Mode != EBlendViewTransformMode::Mirror &&
		!Event.bControlDown &&
		!Event.bAltDown &&
		!Event.bCommandDown &&
		NumericInput.HandleKey(Event.Key))
	{
		ApplyNumericTransform();
		return EBlendViewInputResult::Handled;
	}

	if (Mode != EBlendViewTransformMode::Mirror &&
		Event.Type == EBlendViewInputEventType::KeyDown &&
		Event.Key == EKeys::BackSpace)
	{
		HandleNumericBackspace();
		return EBlendViewInputResult::Handled;
	}

	if (const TOptional<EBlendViewTransformMode> RequestedMode = GetRequestedTransformModeFromCommands(Event);
		RequestedMode.IsSet())
	{
		if (Mode == EBlendViewTransformMode::Rotate &&
			RequestedMode.GetValue() == EBlendViewTransformMode::Rotate)
		{
			EnterFreeRotateMode();
		}
		else
		{
			SetMode(RequestedMode.GetValue());
		}
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type != EBlendViewInputEventType::KeyDown)
	{
		return EBlendViewInputResult::Handled;
	}

	if (Mode != EBlendViewTransformMode::Mirror &&
		FBlendViewCommands::IsRegistered() &&
		FBlendViewCommands::Get().SetSnapBase.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewCommands::Get().SetSnapBase))
	{
		BeginSnapBaseSelection();
		return EBlendViewInputResult::Handled;
	}

	if (Event.Key == EKeys::X)
	{
		if (Event.bShiftDown)
		{
			TogglePlaneConstraint(EBlendViewAxisConstraint::PlaneX);
		}
		else
		{
			ToggleAxisConstraint(EBlendViewAxisConstraint::X);
		}
		return EBlendViewInputResult::Handled;
	}

	if (FBlendViewCommands::IsRegistered() &&
		FBlendViewCommands::Get().TogglePivotEditMode.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewCommands::Get().TogglePivotEditMode))
	{
		TogglePivotEditMode();
		return EBlendViewInputResult::Handled;
	}

	if (FBlendViewCommands::IsRegistered() &&
		FBlendViewCommands::Get().ClearTransformConstraint.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(
			Event,
			*FBlendViewCommands::Get().ClearTransformConstraint))
	{
		ClearConstraint();
		return EBlendViewInputResult::Handled;
	}

	if (Event.Key == EKeys::Y)
	{
		if (Event.bShiftDown)
		{
			TogglePlaneConstraint(EBlendViewAxisConstraint::PlaneY);
		}
		else
		{
			ToggleAxisConstraint(EBlendViewAxisConstraint::Y);
		}
		return EBlendViewInputResult::Handled;
	}

	if (Event.Key == EKeys::Z)
	{
		if (Event.bShiftDown)
		{
			TogglePlaneConstraint(EBlendViewAxisConstraint::PlaneZ);
		}
		else
		{
			ToggleAxisConstraint(EBlendViewAxisConstraint::Z);
		}
		return EBlendViewInputResult::Handled;
	}

	return EBlendViewInputResult::Handled;
}

bool FBlendViewTransformTool::ResetCurrentTransform(const EBlendViewTransformResetChannel Channel)
{
	if (PivotEditSession.IsModelingPivotMode())
	{
		switch (Channel)
		{
		case EBlendViewTransformResetChannel::Location:
		case EBlendViewTransformResetChannel::Rotation:
			return ResetModelingPivotTransformComponent(Channel);
		case EBlendViewTransformResetChannel::Scale:
			return true;
		default:
			return true;
		}
	}

	if (!FBlendViewTransformResetPolicy::DoesChannelMatchMode(Channel, Mode))
	{
		return true;
	}

	RestoreOperationBaselineToOriginal();
	bFreeRotateMode = false;
	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	ClearActiveSnapState();
	PlaneTranslationSession.Reset();
	NumericInput.Clear();
	ResetInputBaselineForMode(Mode, LastMouseViewportPosition);
	UpdateViewportOverlay();
	if (InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->Invalidate();
	}
	return true;
}

void FBlendViewTransformTool::Tick(float DeltaTime)
{
	if ((Mode == EBlendViewTransformMode::Translate || Mode == EBlendViewTransformMode::Mirror) &&
		InitialContext.ViewportClient)
	{
		// The editor viewport recalculates this shared state during normal input handling.
		// Keep the modal transform tool's cursor requirement authoritative for its lifetime.
		InitialContext.ViewportClient->SetRequiredCursorOverride(true, EMouseCursor::CardinalCross);
	}
	if (PivotEditSession.ConsumeModelingPivotInputReanchor())
	{
		RefreshViewportContextAndInputFromCurrentCursor();
	}
	TargetAdapter.RefreshNativeComponentBindings();
	if (PivotEditSession.IsModelingPivotMode() && !TargetAdapter.IsModelingPivotToolStillActive())
	{
		bCompleteRequested = true;
	}
	UpdateViewportOverlay();
	if (OverlaySession.IsAttached())
	{
		ClearWorldPivotVisualization();
	}
	else
	{
		UpdateWorldPivotVisualization();
	}
	if (InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->Invalidate();
	}
}

void FBlendViewTransformTool::DrawHUD(UCanvas* Canvas)
{
	if (OverlaySession.IsAttached())
	{
		return;
	}

	if (!IsCanvasForInitialViewport(Canvas))
	{
		return;
	}

	DrawAxisGuides(Canvas);
	DrawMouseGuide(Canvas);
}

void FBlendViewTransformTool::Confirm()
{
	TargetAdapter.Confirm();
	if (GEditor)
	{
		GEditor->NoteSelectionChange();
		GEditor->RedrawLevelEditingViewports(true);
	}
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform confirm: %s Actors=%d"),
		GetModeName(),
		TargetAdapter.GetActorSnapshots().Num());
}

void FBlendViewTransformTool::Cancel()
{
	ClearActiveSnapState();
	RestoreOperationBaselineToOriginal();
	TargetAdapter.CancelTransaction();
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform cancel: %s Actors=%d"),
		GetModeName(),
		TargetAdapter.GetActorSnapshots().Num());
}

void FBlendViewTransformTool::End()
{
	ResetTransientModalState();
	FBlendViewStatusBarPresenter::Get().Restore();
	RestoreTransformCursorOverride();
	DetachViewportOverlay();
	ClearWorldPivotVisualization();

	TargetAdapter.End();

	if (bViewportWidgetHidden && InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->ShowWidget(bSavedViewportShowWidget);
		bViewportWidgetHidden = false;
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
	}

	if (bSelectionOutlineColorChanged && GEngine)
	{
		GEngine->SetSelectionOutlineColor(SavedSelectionOutlineColor);
		bSelectionOutlineColorChanged = false;
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
	}

	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform end"));
	ClearActiveSnapState();
	ViewportLifetimeGuard.Reset();
}

void FBlendViewTransformTool::PrepareForEngineExit()
{
	// UnrealEd may already be partially torn down, so do not end an outstanding transaction here.
	ResetTransientModalState();
	RestoreTransformCursorOverride();
	TargetAdapter.PrepareForEngineExit();
	ClearActiveSnapState();
	OverlaySession.Reset();
	InitialContext = FBlendViewViewportContext();
	ViewportLifetimeGuard.Reset();
	bViewportWidgetHidden = false;
	bSelectionOutlineColorChanged = false;
}

bool FBlendViewTransformTool::CaptureSelection(const FBlendViewViewportContext& Context)
{
	if (!GEditor)
	{
		return false;
	}

	if (const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>())
	{
		bUseIndividualOrigins =
			Settings->TransformPivotMode == EBlendViewTransformPivotMode::IndividualOrigins;
	}

	const bool bCapturedActiveModelingPivot =
		Context.Kind == EBlendViewViewportKind::LevelEditor &&
		Mode != EBlendViewTransformMode::Mirror &&
		ExplicitComponents.IsEmpty() &&
		TargetAdapter.CaptureActiveModelingPivot(
			Context,
			OriginalPivotLocation,
			LocalConstraintRotation);
	if (bCapturedActiveModelingPivot)
	{
		if (PendingTransaction.IsValid())
		{
			PendingTransaction->Cancel();
			PendingTransaction.Reset();
		}
		PivotEditSession.EnterModelingPivotMode();
		bUseIndividualOrigins = false;
	}
	else if (!TargetAdapter.Capture(
			Context,
			OriginalPivotLocation,
			LocalConstraintRotation,
			MoveTemp(PendingTransaction),
			ExplicitComponents))
	{
		return false;
	}
	OriginalTranslationSnapSourceLocation = OriginalPivotLocation;
	if (!bCapturedActiveModelingPivot &&
		InitialPivotOverride.IsSet() &&
		!InitialPivotOverride.GetValue().ContainsNaN())
	{
		OriginalPivotLocation = InitialPivotOverride.GetValue();
	}
	else if (!bCapturedActiveModelingPivot)
	{
		if (const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>())
		{
			switch (Settings->TransformPivotMode)
			{
			case EBlendViewTransformPivotMode::Cursor:
			{
				FTransform CursorTransform = FTransform::Identity;
			if (FBlendViewSceneCursorState::TryGetSavedTransform(CursorTransform))
				{
					OriginalPivotLocation = CursorTransform.GetLocation();
				}
				break;
			}
			case EBlendViewTransformPivotMode::ActiveItem:
				TryResolveActiveActorPivot(TargetAdapter, OriginalPivotLocation, LocalConstraintRotation);
				break;
			case EBlendViewTransformPivotMode::WorldOrigin:
				OriginalPivotLocation = FVector::ZeroVector;
				break;
			default:
				break;
			}
		}
	}
	InitialPivotOverride.Reset();

	InitialPivotLocation = OriginalPivotLocation;
	CurrentPivotLocation = InitialPivotLocation;
	InitialTranslationSnapSourceLocation = OriginalTranslationSnapSourceLocation;
	FreeMoveIntentPivotLocation = CurrentPivotLocation;
	BaselineFreeMoveIntentPivotLocation = FreeMoveIntentPivotLocation;
	bSavedViewportShowWidget = Context.ViewportClient->GetShowWidget();
	Context.ViewportClient->ShowWidget(false);
	bViewportWidgetHidden = true;
	if (GEngine)
	{
		SavedSelectionOutlineColor = GEngine->GetSelectionOutlineColor();
		GEngine->SetSelectionOutlineColor(FLinearColor::White);
		bSelectionOutlineColorChanged = true;
	}
	if (Context.Kind == EBlendViewViewportKind::LevelEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
	InitialMouseViewportPosition = Context.MouseViewportPosition;
	LastMouseViewportPosition = InitialMouseViewportPosition;
	VirtualPointer.Reset(InitialMouseViewportPosition);
	PlaneTranslationSession.Reset();
	bHasPivotViewportPosition = ProjectWorldToViewport(InitialPivotLocation, PivotViewportPosition);
	bHasInitialMouseWorldLocation = GetMousePlaneIntersection(InitialMouseViewportPosition, InitialMouseWorldLocation);
	MouseGuideWorldLocation = bHasInitialMouseWorldLocation ? InitialMouseWorldLocation : InitialPivotLocation;
	bHasMouseGuideWorldLocation = bHasInitialMouseWorldLocation;
	RotationAccumulator.Reset();
	if (Mode == EBlendViewTransformMode::Rotate && bHasInitialMouseWorldLocation)
	{
		RotationAccumulator.Begin(InitialPivotLocation - InitialMouseWorldLocation);
	}
	if (Mode == EBlendViewTransformMode::Scale)
	{
		ResetScaleSpringInput(InitialMouseViewportPosition);
	}
	else
	{
		ScaleSpring.Reset();
	}
	return true;
}

bool FBlendViewTransformTool::UpdateTranslation(const FBlendViewInputEvent& Event)
{
	const bool bSnapActive = ShouldUseTranslationSnap(Event.bControlDown);
	TGuardValue<bool> SnapGuard(SnapSession.bActiveForCurrentTranslation, bSnapActive);
	if (!bSnapActive)
	{
		ClearActiveSnapState();
	}

	FVector2D CurrentViewportPosition;
	if (!AdvanceVirtualMousePosition(Event, CurrentViewportPosition))
	{
		return false;
	}
	LastMouseViewportPosition = CurrentViewportPosition;

	if (bMiddleMouseAxisSelection)
	{
		UpdateMiddleMouseAxisSelection(CurrentViewportPosition);
	}
	if (IsPlaneConstraint(AxisConstraint))
	{
		return UpdatePlaneTranslationConstraint(CurrentViewportPosition);
	}

	if (!bHasInitialMouseWorldLocation)
	{
		return false;
	}

	FVector CurrentMouseWorldLocation;
	if (!GetMousePlaneIntersection(CurrentViewportPosition, CurrentMouseWorldLocation))
	{
		return false;
	}

	const FVector FreeMoveDelta = CurrentMouseWorldLocation - InitialMouseWorldLocation;
	FreeMoveIntentPivotLocation = BaselineFreeMoveIntentPivotLocation + FreeMoveDelta;

	if (bMiddleMouseAxisSelection)
	{
		if (IsSingleAxisConstraint(AxisConstraint))
		{
			return ProjectFreeMoveIntentToAxis(AxisConstraint, AxisConstraintSign, bLocalConstraint);
		}
	}

	if (IsSingleAxisConstraint(AxisConstraint))
	{
		return ProjectFreeMoveIntentToAxis(AxisConstraint, AxisConstraintSign, bLocalConstraint);
	}
	ApplyTranslationDelta(FreeMoveDelta);
	FreeMoveIntentPivotLocation = CurrentPivotLocation;
	return true;
}

bool FBlendViewTransformTool::BeginSnapBaseSelection()
{
	if (!InitialContext.IsValid())
	{
		return false;
	}

	FrozenTransformValueText = BuildTransformValueText();
	RestoreOperationBaselineToOriginal();
	NumericInput.Clear();
	ClearActiveSnapState();
	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	ResetInputBaselineForMode(Mode, LastMouseViewportPosition);

	SnapSession.BeginBaseSelection();
	UpdateSnapBaseCandidateFromViewport(LastMouseViewportPosition);
	if (!SnapSession.bHasBaseCandidate)
	{
		SnapSession.SetBaseCandidate(CurrentPivotLocation, EBlendViewSnapTargetKind::None);
	}
	return true;
}

bool FBlendViewTransformTool::UpdateSnapBaseCandidateFromEvent(const FBlendViewInputEvent& Event)
{
	if (Event.Type == EBlendViewInputEventType::MouseMove)
	{
		// Keep snap-base picking in the same viewport-relative coordinate stream as
		// normal transforms. Reconstructing from desktop coordinates can introduce
		// a DPI/window offset in restored floating asset-editor windows.
		VirtualPointer.Advance(Event.CursorDelta, 1.0);
	}

	LastMouseViewportPosition = VirtualPointer.GetPosition();
	return UpdateSnapBaseCandidateFromViewport(LastMouseViewportPosition);
}

bool FBlendViewTransformTool::UpdateSnapBaseCandidateFromViewport(const FVector2D& ViewportPosition)
{
	FVector CandidateLocation;
	EBlendViewSnapTargetKind CandidateKind = EBlendViewSnapTargetKind::None;
	if (!GetSnapBaseCandidateFromViewport(ViewportPosition, CandidateLocation, CandidateKind))
	{
		return false;
	}

	SnapSession.SetBaseCandidate(CandidateLocation, CandidateKind);
	return true;
}

bool FBlendViewTransformTool::GetSnapBaseCandidateFromViewport(
	const FVector2D& ViewportPosition,
	FVector& OutCandidate,
	EBlendViewSnapTargetKind& OutKind)
{
	if (!InitialContext.IsValid())
	{
		return false;
	}

	FBlendViewSnapCandidate SnapCandidate;
	FBlendViewSnapQuery SnapQuery;
	SnapQuery.ViewportPosition = ViewportPosition;
	FVector MousePlaneLocation = CurrentPivotLocation;
	if (GetMousePlaneIntersection(ViewportPosition, MousePlaneLocation))
	{
		SnapQuery.SourceAfterDelta = MousePlaneLocation;
		SnapQuery.TargetHintLocation = MousePlaneLocation;
		SnapQuery.bHasTargetHintLocation = true;
	}
	else
	{
		SnapQuery.SourceAfterDelta = CurrentPivotLocation;
	}
	SnapQuery.ViewportClient = InitialContext.ViewportClient;
	SnapQuery.GridSizeUnrealUnits = FBlendViewSnapSolver::GetTemporaryTranslationGridSize(ShouldUseUnrealEditorSnap());
	SnapQuery.ScreenRadiusScale = GetTransformSnapRadiusScale();
	ApplySnapTargetFilters(SnapQuery, false, false);
	SnapQuery.GetViewportTraceSegment = [this](
		const FVector2D& InViewportPosition,
		FVector& OutTraceStart,
		FVector& OutTraceEnd)
	{
		return GetViewportTraceSegment(InViewportPosition, OutTraceStart, OutTraceEnd);
	};
	SnapQuery.ProjectWorldToViewport = [this](const FVector& WorldPosition, FVector2D& OutViewportPosition)
	{
		return ProjectWorldToViewport(WorldPosition, OutViewportPosition);
	};
	if (SnapSolver.FindTemporarySnapTarget(SnapQuery, SnapCandidate))
	{
		OutCandidate = SnapCandidate.Location;
		OutKind = SnapCandidate.Kind;
		return true;
	}

	return false;
}

void FBlendViewTransformTool::ConfirmSnapBaseSelection()
{
	SnapSession.ConfirmBaseSelection();
	FrozenTransformValueText.Reset();
	ClearActiveSnapState();
	if (SnapSession.bHasBase && Mode == EBlendViewTransformMode::Translate)
	{
		FreeMoveIntentPivotLocation = CurrentPivotLocation;
		BaselineFreeMoveIntentPivotLocation = CurrentPivotLocation;
		ResetInputBaselineForMode(Mode, LastMouseViewportPosition);
	}
}

void FBlendViewTransformTool::CancelSnapBaseSelection()
{
	FrozenTransformValueText.Reset();
	SnapSession.CancelBaseSelection();
	ClearActiveSnapState();
}

bool FBlendViewTransformTool::BeginMiddleMouseAxisSelection(const FBlendViewInputEvent& Event)
{
	bHasPivotViewportPosition = ProjectWorldToViewport(CurrentPivotLocation, PivotViewportPosition);
	if (!bHasPivotViewportPosition)
	{
		UE_LOG(LogBlendViewTransform, Warning,
			TEXT("BlendView MMB axis selection ignored: current pivot projection failed Pivot=%s"),
			*CurrentPivotLocation.ToString());
		return false;
	}

	const FVector2D ViewportPosition = VirtualPointer.GetPosition();
	LastMouseViewportPosition = ViewportPosition;
	bFreeRotateMode = false;
	bMiddleMouseAxisSelection = true;
	bMiddleMousePlaneSelection = Event.bShiftDown;
	bLocalConstraint = false;
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView MMB axis selection begin"));

	if (Mode != EBlendViewTransformMode::Translate &&
		Mode != EBlendViewTransformMode::Mirror)
	{
		RestoreOperationBaselineToOriginal();
		// MMB reanchors constraint selection, but Blender's scale spring remains anchored to S start.
		ResetInputBaselineForMode(Mode, ViewportPosition, EScaleSpringResetPolicy::Preserve);
	}

	UpdateMiddleMouseAxisSelection(ViewportPosition);
	if (Mode == EBlendViewTransformMode::Translate && IsSingleAxisConstraint(AxisConstraint))
	{
		ProjectFreeMoveIntentToAxis(AxisConstraint, AxisConstraintSign, false);
	}
	else if (Mode == EBlendViewTransformMode::Translate && IsPlaneConstraint(AxisConstraint))
	{
		BeginPlaneTranslationConstraint(AxisConstraint, ViewportPosition, false);
	}
	else if (Mode == EBlendViewTransformMode::Mirror)
	{
		ApplyMirrorTransform();
	}
	return true;
}

bool FBlendViewTransformTool::EndMiddleMouseAxisSelection(const FBlendViewInputEvent& Event)
{
	if (!bMiddleMouseAxisSelection)
	{
		return false;
	}

	const FVector2D ViewportPosition = VirtualPointer.GetPosition();
	LastMouseViewportPosition = ViewportPosition;

	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView MMB axis selection end"));
	return true;
}

bool FBlendViewTransformTool::UpdateMiddleMouseAxisSelection(const FVector2D& ViewportPosition)
{
	if (!bHasPivotViewportPosition)
	{
		return false;
	}

	FVector2D ConstraintOriginViewportPosition;
	FVector2D GuideViewportPosition;
	if (!GetMiddleMouseGuideViewportSegment(ViewportPosition, ConstraintOriginViewportPosition, GuideViewportPosition) ||
		(GuideViewportPosition - ConstraintOriginViewportPosition).SizeSquared() < 1.0)
	{
		return false;
	}

	LastMouseViewportPosition = ViewportPosition;

	double BestScore = -2.0;
	EBlendViewAxisConstraint BestAxis = AxisConstraint;
	double BestSign = AxisConstraintSign;
	for (const EBlendViewAxisConstraint CandidateAxis : {
		EBlendViewAxisConstraint::X,
		EBlendViewAxisConstraint::Y,
		EBlendViewAxisConstraint::Z})
	{
		double WorldDistance = 0.0;
		double CandidateSign = 1.0;
		double Alignment = -1.0;
		if (!ProjectViewportGuideToAxis(CandidateAxis, GuideViewportPosition, WorldDistance, CandidateSign, Alignment))
		{
			continue;
		}

		if (Alignment > BestScore)
		{
			BestScore = Alignment;
			BestAxis = CandidateAxis;
			BestSign = CandidateSign;
		}
	}

	if (BestScore <= -2.0)
	{
		return false;
	}

	const EBlendViewAxisConstraint TargetConstraint =
		bMiddleMousePlaneSelection
		? AxisToPlaneConstraint(BestAxis)
		: BestAxis;
	return SetAxisConstraint(TargetConstraint, BestSign, false);
}

bool FBlendViewTransformTool::GetMiddleMouseGuideViewportSegment(
	const FVector2D& ViewportPosition,
	FVector2D& OutStart,
	FVector2D& OutEnd) const
{
	if (!ProjectWorldToViewport(OriginalPivotLocation, OutStart))
	{
		return false;
	}

	if (Mode == EBlendViewTransformMode::Translate)
	{
		return ProjectWorldToViewport(FreeMoveIntentPivotLocation, OutEnd);
	}

	// Blender's constraint-select guide is based on mouse travel from the
	// transform start, not on the pivot-to-pointer vector.
	const FVector2D MouseTravel = ViewportPosition - InitialMouseViewportPosition;
	if (MouseTravel.SizeSquared() < 1.0)
	{
		return false;
	}

	OutEnd = OutStart + MouseTravel;
	return true;
}

bool FBlendViewTransformTool::GetMiddleMouseGuideOverlaySegment(
	const FVector2D& ViewportPosition,
	FVector2D& OutStart,
	FVector2D& OutEnd) const
{
	if (InitialContext.ViewportSize.X <= 0 || InitialContext.ViewportSize.Y <= 0)
	{
		return false;
	}

	FVector2D ViewportStart;
	FVector2D ViewportEnd;
	if (!GetMiddleMouseGuideViewportSegment(ViewportPosition, ViewportStart, ViewportEnd))
	{
		return false;
	}

	const FVector2D ViewportSize(
		static_cast<double>(InitialContext.ViewportSize.X),
		static_cast<double>(InitialContext.ViewportSize.Y));
	OutStart = ViewportStart / ViewportSize;
	OutEnd = ViewportEnd / ViewportSize;
	return true;
}

bool FBlendViewTransformTool::GetMiddleMouseGuideCanvasSegment(
	UCanvas* Canvas,
	const FVector2D& ViewportPosition,
	FVector2D& OutStart,
	FVector2D& OutEnd) const
{
	if (!Canvas || InitialContext.ViewportSize.X <= 0 || InitialContext.ViewportSize.Y <= 0)
	{
		return false;
	}

	FVector2D ViewportStart;
	FVector2D ViewportEnd;
	if (!GetMiddleMouseGuideViewportSegment(ViewportPosition, ViewportStart, ViewportEnd))
	{
		return false;
	}

	const FVector2D CanvasScale(
		static_cast<double>(Canvas->SizeX) / static_cast<double>(InitialContext.ViewportSize.X),
		static_cast<double>(Canvas->SizeY) / static_cast<double>(InitialContext.ViewportSize.Y));
	OutStart = ViewportStart * CanvasScale;
	OutEnd = ViewportEnd * CanvasScale;
	return true;
}

bool FBlendViewTransformTool::UpdateRotation(const FBlendViewInputEvent& Event)
{
	if (!bHasPivotViewportPosition || !bHasInitialMouseWorldLocation)
	{
		return false;
	}

	FVector2D CurrentViewportPosition;
	if (!AdvanceVirtualMousePosition(Event, CurrentViewportPosition))
	{
		return false;
	}
	const FVector2D ViewportDelta = CurrentViewportPosition - LastMouseViewportPosition;
	LastMouseViewportPosition = CurrentViewportPosition;
	TGuardValue<bool> IncrementSnapGuard(
		bIncrementSnapActiveForCurrentTransform,
		ShouldUseIncrementSnap(Event.bControlDown));
	if (bFreeRotateMode)
	{
		FreeRotateAccumulatedViewportDelta += ViewportDelta;
		return ApplyFreeRotationFromViewportDelta(FreeRotateAccumulatedViewportDelta);
	}
	return ApplyRotationFromViewportPosition(CurrentViewportPosition);
}

bool FBlendViewTransformTool::RefreshRotationFromCurrentIntent(const bool bIncrementSnapActive)
{
	if (Mode != EBlendViewTransformMode::Rotate)
	{
		return false;
	}

	TGuardValue<bool> IncrementSnapGuard(
		bIncrementSnapActiveForCurrentTransform,
		bIncrementSnapActive && !NumericInput.IsActive());
	return ApplyRotationFromViewportPosition(LastMouseViewportPosition);
}

bool FBlendViewTransformTool::ApplyRotationFromViewportPosition(const FVector2D& ViewportPosition)
{
	FVector CurrentMouseWorldLocation;
	if (!GetMousePlaneIntersection(ViewportPosition, CurrentMouseWorldLocation))
	{
		return false;
	}

	if (ShouldDrawMouseGuide())
	{
		MouseGuideWorldLocation = CurrentMouseWorldLocation;
		bHasMouseGuideWorldLocation = true;
	}

	if (bMiddleMouseAxisSelection)
	{
		UpdateMiddleMouseAxisSelection(ViewportPosition);
	}

	const FVector Pivot = InitialPivotLocation;
	const FVector CurrentDirection = (Pivot - CurrentMouseWorldLocation).GetSafeNormal();
	const FVector InitialDirection = (Pivot - InitialMouseWorldLocation).GetSafeNormal();
	const FVector RotationAxis = GetRotationAxisVector();
	if (CurrentDirection.IsNearlyZero() || InitialDirection.IsNearlyZero() || RotationAxis.IsNearlyZero())
	{
		return false;
	}

	const FVector ViewDirection =
		BlendViewViewportCompat::GetForwardVector(*InitialContext.ViewportClient);
	const FVector SignedViewNormal = ViewDirection *
		(FVector::DotProduct(ViewDirection, RotationAxis) < 0.0 ? -1.0 : 1.0);
	double AngleRadians = 0.0;
	if (!RotationAccumulator.Evaluate(CurrentDirection, SignedViewNormal, AngleRadians))
	{
		return false;
	}
	ApplyRotationDelta(IsPlaneConstraint(AxisConstraint) ? -AngleRadians : AngleRadians);
	return true;
}

bool FBlendViewTransformTool::ApplyFreeRotationFromViewportDelta(const FVector2D& ViewportDelta)
{
	if (!InitialContext.IsValid() || ViewportDelta.IsNearlyZero())
	{
		return false;
	}

	FVector ViewRight;
	FVector ViewUp;
	if (InitialContext.ViewportClient->IsPerspective())
	{
		const FRotationMatrix ViewRotationMatrix(InitialContext.ViewportClient->GetViewRotation());
		ViewRight = ViewRotationMatrix.GetScaledAxis(EAxis::Y).GetSafeNormal();
		ViewUp = ViewRotationMatrix.GetScaledAxis(EAxis::Z).GetSafeNormal();
	}
	else
	{
		FVector ViewForward;
		if (!FBlendViewViewportProjector::GetViewBasis(
			InitialContext,
			ViewForward,
			ViewRight,
			ViewUp))
		{
			return false;
		}
	}
	if (ViewRight.IsNearlyZero() || ViewUp.IsNearlyZero())
	{
		return false;
	}

	constexpr double TrackballRadiansPerPixel = 0.01;
	const FQuat HorizontalRotation(ViewUp, -ViewportDelta.X * TrackballRadiansPerPixel);
	const FQuat VerticalRotation(ViewRight, -ViewportDelta.Y * TrackballRadiansPerPixel);
	ApplyFreeRotationDelta((VerticalRotation * HorizontalRotation).GetNormalized());
	return true;
}

bool FBlendViewTransformTool::UpdateScale(const FBlendViewInputEvent& Event)
{
	if (!bHasPivotViewportPosition)
	{
		return false;
	}

	FVector2D CurrentViewportPosition;
	if (!AdvanceVirtualMousePosition(Event, CurrentViewportPosition))
	{
		return false;
	}
	LastMouseViewportPosition = CurrentViewportPosition;
	TGuardValue<bool> IncrementSnapGuard(
		bIncrementSnapActiveForCurrentTransform,
		ShouldUseIncrementSnap(Event.bControlDown));
	TGuardValue<bool> PrecisionGuard(
		bPrecisionModeActiveForCurrentTransform,
		Event.bShiftDown);
	return ApplyScaleFromViewportPosition(CurrentViewportPosition);
}

bool FBlendViewTransformTool::RefreshScaleFromCurrentIntent(
	const bool bIncrementSnapActive,
	const bool bPrecisionModeActive)
{
	if (Mode != EBlendViewTransformMode::Scale)
	{
		return false;
	}

	TGuardValue<bool> IncrementSnapGuard(
		bIncrementSnapActiveForCurrentTransform,
		bIncrementSnapActive && !NumericInput.IsActive());
	TGuardValue<bool> PrecisionGuard(
		bPrecisionModeActiveForCurrentTransform,
		bPrecisionModeActive);
	return ApplyScaleFromViewportPosition(LastMouseViewportPosition);
}

bool FBlendViewTransformTool::ApplyScaleFromViewportPosition(const FVector2D& ViewportPosition)
{
	FVector CurrentMouseWorldLocation;
	if (GetMousePlaneIntersection(ViewportPosition, CurrentMouseWorldLocation))
	{
		MouseGuideWorldLocation = CurrentMouseWorldLocation;
		bHasMouseGuideWorldLocation = true;
	}

	if (bMiddleMouseAxisSelection)
	{
		UpdateMiddleMouseAxisSelection(ViewportPosition);
	}

	double ScaleFactor = 1.0;
	if (!ScaleSpring.Evaluate(ViewportPosition, ScaleFactor))
	{
		return false;
	}

	ApplyScaleFactor(ScaleFactor);
	return true;
}

bool FBlendViewTransformTool::UpdateMirror(const FBlendViewInputEvent& Event)
{
	FVector2D CurrentViewportPosition;
	if (!AdvanceVirtualMousePosition(Event, CurrentViewportPosition))
	{
		return false;
	}
	LastMouseViewportPosition = CurrentViewportPosition;

	if (bMiddleMouseAxisSelection)
	{
		if (UpdateMiddleMouseAxisSelection(CurrentViewportPosition))
		{
			ApplyMirrorTransform();
			return true;
		}
	}

	return true;
}

void FBlendViewTransformTool::ResetScaleSpringInput(const FVector2D& ViewportPosition)
{
	ScaleSpring.Reset();
	FVector2D ScalePivotViewportPosition;
	if (!ProjectWorldToViewport(OriginalPivotLocation, ScalePivotViewportPosition))
	{
		return;
	}
	ScaleSpring.Begin(ScalePivotViewportPosition, ViewportPosition);
}

bool FBlendViewTransformTool::AdvanceVirtualMousePosition(
	const FBlendViewInputEvent& Event,
	FVector2D& OutViewportPosition)
{
	if (Event.Type != EBlendViewInputEventType::MouseMove)
	{
		return false;
	}

	const double PrecisionScale = Mode == EBlendViewTransformMode::Rotate
		? RotationPrecisionMouseScale
		: PrecisionMouseScale;
	const double MouseScale = Event.bShiftDown ? PrecisionScale : 1.0;
	OutViewportPosition = VirtualPointer.Advance(Event.CursorDelta, MouseScale);
	return true;
}

bool FBlendViewTransformTool::RefreshViewportContextAndInputFromCurrentCursor()
{
	if (!FSlateApplication::IsInitialized() || !InitialContext.IsValid())
	{
		return false;
	}

	const FVector2D ScreenPosition = FSlateApplication::Get().GetCursorPos();
	FBlendViewViewportResolver ViewportResolver;
	FBlendViewViewportContext RefreshedContext = ViewportResolver.ResolveForMousePosition(ScreenPosition);
	if (RefreshedContext.IsValid() && RefreshedContext.ViewportClient == InitialContext.ViewportClient)
	{
		InitialContext = RefreshedContext;
		ResetInputBaselineForMode(Mode, RefreshedContext.MouseViewportPosition);
		return true;
	}

	if (!InitialContext.SceneViewport)
	{
		return false;
	}

	const FIntPoint ViewportSize = InitialContext.SceneViewport->GetSizeXY();
	FVector2D MouseViewportPosition = FVector2D::ZeroVector;
	if (!FBlendViewViewportGeometry::TryScreenToViewportPosition(
			InitialContext.ViewportWidget.Pin(),
			ScreenPosition,
			ViewportSize,
			MouseViewportPosition))
	{
		return false;
	}

	InitialContext.MouseScreenPosition = ScreenPosition;
	InitialContext.ViewportSize = ViewportSize;
	InitialContext.MouseViewportPosition = MouseViewportPosition;
	ResetInputBaselineForMode(Mode, InitialContext.MouseViewportPosition);
	return true;
}

bool FBlendViewTransformTool::ProjectWorldToViewport(
	const FVector& WorldPosition,
	FVector2D& OutViewportPosition) const
{
	return FBlendViewViewportProjector::ProjectWorldToViewport(
		InitialContext,
		WorldPosition,
		OutViewportPosition);
}

bool FBlendViewTransformTool::GetMousePlaneIntersection(
	const FVector2D& ViewportPosition,
	FVector& OutIntersection) const
{
	FVector RayStart;
	FVector RayDirection;
	if (!GetViewportRay(ViewportPosition, RayStart, RayDirection))
	{
		return false;
	}

	const FVector RayEnd = RayStart + RayDirection * 1000000.0;
	const FVector ViewNormal =
		BlendViewViewportCompat::GetForwardVector(*InitialContext.ViewportClient);
	if (FMath::IsNearlyZero(FVector::DotProduct(RayDirection, ViewNormal)))
	{
		return false;
	}

	OutIntersection = FMath::LinePlaneIntersection(
		RayStart,
		RayEnd,
		InitialPivotLocation,
		ViewNormal);
	return true;
}

bool FBlendViewTransformTool::GetViewportRay(
	const FVector2D& ViewportPosition,
	FVector& OutRayStart,
	FVector& OutRayDirection) const
{
	return FBlendViewViewportProjector::GetViewportRay(
		InitialContext,
		ViewportPosition,
		OutRayStart,
		OutRayDirection);
}

bool FBlendViewTransformTool::GetViewportTraceSegment(
	const FVector2D& ViewportPosition,
	FVector& OutTraceStart,
	FVector& OutTraceEnd) const
{
	return FBlendViewViewportProjector::GetViewportTraceSegment(
		InitialContext,
		ViewportPosition,
		OutTraceStart,
		OutTraceEnd);
}

bool FBlendViewTransformTool::ProjectViewportGuideToAxis(
	const EBlendViewAxisConstraint Axis,
	const FVector2D& ViewportPosition,
	double& OutWorldDistance,
	double& OutSign,
	double& OutAlignment) const
{
	OutWorldDistance = 0.0;
	OutSign = 1.0;
	OutAlignment = -1.0;

	const FVector AxisVector = GetAxisBaseVector(Axis);
	if (AxisVector.IsNearlyZero())
	{
		return false;
	}

	FVector2D OriginViewportPosition;
	if (!ProjectWorldToViewport(OriginalPivotLocation, OriginViewportPosition))
	{
		return false;
	}

	const double AxisProjectionLength = GetAxisProjectionWorldDistance();
	FVector2D AxisViewportPosition;
	FVector2D AxisScreenVector;
	if (ProjectWorldToViewport(OriginalPivotLocation + AxisVector * AxisProjectionLength, AxisViewportPosition))
	{
		AxisScreenVector = AxisViewportPosition - OriginViewportPosition;
	}
	else if (ProjectWorldToViewport(OriginalPivotLocation - AxisVector * AxisProjectionLength, AxisViewportPosition))
	{
		AxisScreenVector = OriginViewportPosition - AxisViewportPosition;
	}
	else
	{
		return false;
	}

	const double AxisScreenLength = AxisScreenVector.Size();
	if (AxisScreenLength < 2.0)
	{
		return false;
	}

	const FVector2D GuideScreenVector = ViewportPosition - OriginViewportPosition;
	const double GuideScreenLength = GuideScreenVector.Size();
	if (GuideScreenLength < 2.0)
	{
		return false;
	}

	const FVector2D AxisDirection = AxisScreenVector / AxisScreenLength;
	const double SignedScreenDistance = FVector2D::DotProduct(GuideScreenVector, AxisDirection);
	OutAlignment = FMath::Abs(SignedScreenDistance) / GuideScreenLength;
	if (FMath::Abs(SignedScreenDistance) < UE_SMALL_NUMBER)
	{
		OutWorldDistance = 0.0;
		OutSign = SignedScreenDistance < 0.0 ? -1.0 : 1.0;
		return true;
	}

	FVector RayStart;
	FVector RayDirection;
	if (!GetViewportRay(
		OriginViewportPosition + AxisDirection * SignedScreenDistance,
		RayStart,
		RayDirection))
	{
		return false;
	}

	const FVector AxisDirectionWorld = AxisVector.GetSafeNormal();
	const FVector RayToAxisOrigin = RayStart - OriginalPivotLocation;
	const double RayDotRay = FVector::DotProduct(RayDirection, RayDirection);
	const double RayDotAxis = FVector::DotProduct(RayDirection, AxisDirectionWorld);
	const double AxisDotAxis = FVector::DotProduct(AxisDirectionWorld, AxisDirectionWorld);
	const double RayDotOrigin = FVector::DotProduct(RayDirection, RayToAxisOrigin);
	const double AxisDotOrigin = FVector::DotProduct(AxisDirectionWorld, RayToAxisOrigin);
	const double Denominator = RayDotRay * AxisDotAxis - RayDotAxis * RayDotAxis;
	if (FMath::Abs(Denominator) <= UE_SMALL_NUMBER)
	{
		OutWorldDistance = SignedScreenDistance / (AxisScreenLength / AxisProjectionLength);
	}
	else
	{
		OutWorldDistance = (RayDotRay * AxisDotOrigin - RayDotAxis * RayDotOrigin) / Denominator;
	}
	OutSign = OutWorldDistance < 0.0 ? -1.0 : 1.0;
	return true;
}

double FBlendViewTransformTool::GetAxisProjectionWorldDistance() const
{
	if (!InitialContext.IsValid())
	{
		return MinAxisDirectionProbeWorldDistance;
	}

	const double CameraDistance = FVector::Dist(
		InitialContext.ViewportClient->GetViewLocation(),
		OriginalPivotLocation);
	return FMath::Max(CameraDistance * 0.2, MinAxisDirectionProbeWorldDistance);
}

bool FBlendViewTransformTool::ProjectWorldToCanvas(
	UCanvas* Canvas,
	const FVector& WorldPosition,
	FVector2D& OutCanvasPosition) const
{
	return FBlendViewViewportProjector::ProjectWorldToCanvas(
		Canvas,
		WorldPosition,
		OutCanvasPosition);
}

bool FBlendViewTransformTool::IsCanvasForInitialViewport(UCanvas* Canvas) const
{
	return FBlendViewViewportProjector::IsCanvasForViewport(InitialContext, Canvas);
}

bool FBlendViewTransformTool::ProjectWorldToOverlay(
	const FVector& WorldPosition,
	FVector2D& OutNormalizedPosition) const
{
	return FBlendViewViewportProjector::ProjectWorldToOverlay(
		InitialContext,
		WorldPosition,
		OutNormalizedPosition);
}

void FBlendViewTransformTool::AttachViewportOverlay()
{
	if (OverlaySession.IsAttached() || !InitialContext.ViewportClient)
	{
		return;
	}

	TSharedPtr<SLevelViewport> LevelViewport;
	if (InitialContext.Kind == EBlendViewViewportKind::LevelEditor &&
		FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		if (TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor())
		{
			for (const TSharedPtr<SLevelViewport>& CandidateViewport : LevelEditor->GetViewports())
			{
				if (CandidateViewport.IsValid() &&
					&CandidateViewport->GetLevelViewportClient() == InitialContext.ViewportClient)
				{
					LevelViewport = CandidateViewport;
					break;
				}
			}
		}
	}

	OverlaySession.Attach(
		LevelViewport,
		InitialContext.ViewportWidget.Pin(),
		InitialContext.SceneViewport);
}

void FBlendViewTransformTool::DetachViewportOverlay()
{
	OverlaySession.Detach();
}

void FBlendViewTransformTool::ApplyTransformCursorOverride()
{
	if (!OverlaySession.IsAttached() ||
		!FSlateApplication::IsInitialized())
	{
		return;
	}
	if (Mode == EBlendViewTransformMode::Translate ||
		Mode == EBlendViewTransformMode::Mirror)
	{
		if (InitialContext.ViewportClient)
		{
			InitialContext.ViewportClient->SetRequiredCursorOverride(true, EMouseCursor::CardinalCross);
		}
		FSlateApplication::Get().QueryCursor();
		return;
	}
	if (bCursorVisibilityOverridden)
	{
		return;
	}
	if (InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->SetRequiredCursorOverride(false);
	}

	if (const TSharedPtr<FSlateUser> CursorUser = FSlateApplication::Get().GetCursorUser())
	{
		bSavedCursorVisible = CursorUser->IsCursorVisible();
		CursorUser->SetCursorVisibility(false);
		bCursorVisibilityOverridden = true;
	}
}

void FBlendViewTransformTool::RestoreTransformCursorOverride()
{
	if (InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->SetRequiredCursorOverride(false);
	}
	if (FSlateApplication::IsInitialized())
	{
		if (bCursorVisibilityOverridden)
		{
			if (const TSharedPtr<FSlateUser> CursorUser = FSlateApplication::Get().GetCursorUser())
			{
				CursorUser->SetCursorVisibility(bSavedCursorVisible);
			}
		}
		FSlateApplication::Get().QueryCursor();
	}
	bCursorVisibilityOverridden = false;
}

void FBlendViewTransformTool::UpdateViewportOverlay()
{
	TOptional<FBlendViewStatusLine> StatusLine;
	if (FBlendViewSessionState::AreStatusHintsVisible())
	{
		StatusLine = BuildStatusLine();
	}
	FBlendViewStatusBarPresenter::Get().Present(
		StatusLine,
		InitialContext.ViewportWidget.IsValid()
			? InitialContext.ViewportWidget.Pin()
			: StaticCastSharedPtr<SWidget>(OverlaySession.GetLevelViewport()));

	TSharedPtr<SBlendViewTransformOverlay> Overlay = OverlaySession.GetOverlay();
	if (!Overlay.IsValid())
	{
		return;
	}
	Overlay->SetValueText(
		FrozenTransformValueText.IsSet()
			? FrozenTransformValueText.GetValue()
			: BuildTransformValueText());

	const FIntPoint CurrentViewportSize = InitialContext.Viewport
		? InitialContext.Viewport->GetSizeXY()
		: FIntPoint::ZeroValue;
	if (CurrentViewportSize.X > 0 && CurrentViewportSize.Y > 0)
	{
		const FVector2D ViewportSize(
			static_cast<double>(CurrentViewportSize.X),
			static_cast<double>(CurrentViewportSize.Y));
		FVector2D CursorPivot;
		if (!ProjectWorldToOverlay(CurrentPivotLocation, CursorPivot))
		{
			CursorPivot = LastMouseViewportPosition / ViewportSize;
		}
		Overlay->SetCursorData(
			Mode,
			CursorPivot,
			LastMouseViewportPosition / ViewportSize,
			Mode == EBlendViewTransformMode::Rotate && bFreeRotateMode);
	}
	else
	{
		Overlay->ClearCursorData();
	}

	FVector2D AxisPivot;
	FVector2D PivotPoint;
	if (!ProjectWorldToOverlay(CurrentPivotLocation, PivotPoint))
	{
		Overlay->ClearDrawingData();
		return;
	}
	const bool bHasAxisPivot = ProjectWorldToOverlay(OriginalPivotLocation, AxisPivot);
	if (!bHasAxisPivot)
	{
		AxisPivot = PivotPoint;
	}

	TArray<FBlendViewTransformOverlayAxis> Axes;
	auto AddOverlayAxis = [&](const EBlendViewAxisConstraint Axis, const bool bIsActive)
	{
		if (!bHasAxisPivot)
		{
			return;
		}

		const FVector WorldAxis = GetAxisBaseVector(Axis);
		if (WorldAxis.IsNearlyZero())
		{
			return;
		}

		const double DirectionSign = bIsActive ? AxisConstraintSign : 1.0;
		const double ProjectionDistance = GetAxisProjectionWorldDistance();
		FVector2D Probe;
		FVector2D Direction;
		if (ProjectWorldToOverlay(OriginalPivotLocation + WorldAxis * DirectionSign * ProjectionDistance, Probe))
		{
			Direction = Probe - AxisPivot;
		}
		else if (ProjectWorldToOverlay(OriginalPivotLocation - WorldAxis * DirectionSign * ProjectionDistance, Probe))
		{
			Direction = AxisPivot - Probe;
		}
		else
		{
			return;
		}

		if (Direction.IsNearlyZero())
		{
			return;
		}

		if (bMiddleMouseAxisSelection && !bIsActive)
		{
			FVector2D GuideStart;
			FVector2D GuideEnd;
			if (GetMiddleMouseGuideOverlaySegment(LastMouseViewportPosition, GuideStart, GuideEnd))
			{
				if (FVector2D::DotProduct(GuideEnd - GuideStart, Direction) < 0.0)
				{
					Direction *= -1.0;
				}
			}
		}

		Axes.Add({Direction.GetSafeNormal(), GetBlendViewAxisColor(Axis, bIsActive)});
	};

	if (bMiddleMouseAxisSelection)
	{
		AddOverlayAxis(EBlendViewAxisConstraint::X, IsAxisActiveForCurrentConstraint(EBlendViewAxisConstraint::X));
		AddOverlayAxis(EBlendViewAxisConstraint::Y, IsAxisActiveForCurrentConstraint(EBlendViewAxisConstraint::Y));
		AddOverlayAxis(EBlendViewAxisConstraint::Z, IsAxisActiveForCurrentConstraint(EBlendViewAxisConstraint::Z));
	}
	else if (AxisConstraint != EBlendViewAxisConstraint::None)
	{
		if (AxisConstraint == EBlendViewAxisConstraint::PlaneX)
		{
			AddOverlayAxis(EBlendViewAxisConstraint::Y, true);
			AddOverlayAxis(EBlendViewAxisConstraint::Z, true);
		}
		else if (AxisConstraint == EBlendViewAxisConstraint::PlaneY)
		{
			AddOverlayAxis(EBlendViewAxisConstraint::X, true);
			AddOverlayAxis(EBlendViewAxisConstraint::Z, true);
		}
		else if (AxisConstraint == EBlendViewAxisConstraint::PlaneZ)
		{
			AddOverlayAxis(EBlendViewAxisConstraint::X, true);
			AddOverlayAxis(EBlendViewAxisConstraint::Y, true);
		}
		else
		{
			AddOverlayAxis(AxisConstraint, true);
		}
	}

	TOptional<FVector2D> GuideStart;
	TOptional<FVector2D> GuideEnd;
	TOptional<FVector2D> SnapBaseMarker;
	EBlendViewSnapTargetKind SnapMarkerKind = SnapSession.BaseCandidateKind;
	if (SnapSession.bSelectingBase && SnapSession.bHasBaseCandidate)
	{
		FVector2D ProjectedSnapBaseMarker;
		if (ProjectWorldToOverlay(SnapSession.BaseCandidateLocation, ProjectedSnapBaseMarker))
		{
			SnapBaseMarker = ProjectedSnapBaseMarker;
		}
	}
	else if (SnapSession.bHasActiveTarget &&
		(SnapSession.ActiveTargetKind == EBlendViewSnapTargetKind::Vertex ||
			SnapSession.ActiveTargetKind == EBlendViewSnapTargetKind::EdgeMidpoint ||
			SnapSession.ActiveTargetKind == EBlendViewSnapTargetKind::Edge ||
			SnapSession.ActiveTargetKind == EBlendViewSnapTargetKind::Face))
	{
		FVector2D ProjectedSnapTargetMarker;
		if (ProjectWorldToOverlay(SnapSession.ActiveTargetLocation, ProjectedSnapTargetMarker))
		{
			SnapBaseMarker = ProjectedSnapTargetMarker;
			SnapMarkerKind = SnapSession.ActiveTargetKind;
		}
	}
	if (ShouldDrawMouseGuide())
	{
		if (bMiddleMouseAxisSelection)
		{
			FVector2D ProjectedGuideStart;
			FVector2D ProjectedGuideEnd;
			if (GetMiddleMouseGuideOverlaySegment(LastMouseViewportPosition, ProjectedGuideStart, ProjectedGuideEnd))
			{
				GuideStart = ProjectedGuideStart;
				GuideEnd = ProjectedGuideEnd;
			}
		}
		else
		{
			FVector2D ProjectedGuideEnd;
			if (ProjectWorldToOverlay(MouseGuideWorldLocation, ProjectedGuideEnd))
			{
				GuideStart = PivotPoint;
				GuideEnd = ProjectedGuideEnd;
			}
		}
	}

	Overlay->SetDrawingData(
		AxisPivot,
		PivotPoint,
		Axes,
		GuideStart,
		GuideEnd,
		SnapBaseMarker,
		SnapMarkerKind,
		SavedSelectionOutlineColor,
		!PivotEditSession.IsAnyPivotMode());
}

void FBlendViewTransformTool::UpdateWorldPivotVisualization()
{
	if (PivotEditSession.IsModelingPivotMode() || !InitialContext.IsValid() || !InitialContext.ViewportClient)
	{
		return;
	}
	FBlendViewWorldPivotVisualizer::Draw(
		InitialContext,
		CurrentPivotLocation,
		SavedSelectionOutlineColor);
}

void FBlendViewTransformTool::ClearWorldPivotVisualization()
{
	FBlendViewWorldPivotVisualizer::Clear(InitialContext);
}

bool FBlendViewTransformTool::ShouldDrawMouseGuide() const
{
	if (SnapSession.bSelectingBase)
	{
		return false;
	}

	return (Mode == EBlendViewTransformMode::Translate && bMiddleMouseAxisSelection) ||
		(Mode == EBlendViewTransformMode::Mirror && bMiddleMouseAxisSelection) ||
		(Mode == EBlendViewTransformMode::Rotate && !bFreeRotateMode) ||
		Mode == EBlendViewTransformMode::Scale;
}

void FBlendViewTransformTool::DrawMouseGuide(UCanvas* Canvas) const
{
	if (!ShouldDrawMouseGuide())
	{
		return;
	}

	FVector2D GuideStart;
	FVector2D GuideEnd;
	if (bMiddleMouseAxisSelection)
	{
		if (!GetMiddleMouseGuideCanvasSegment(Canvas, LastMouseViewportPosition, GuideStart, GuideEnd))
		{
			return;
		}
	}
	else if (!bHasMouseGuideWorldLocation ||
		!ProjectWorldToCanvas(Canvas, CurrentPivotLocation, GuideStart) ||
		!ProjectWorldToCanvas(Canvas, MouseGuideWorldLocation, GuideEnd))
	{
		return;
	}

	const FVector2D ShadowOffset(0.0, BlendViewTransformCanvasDrawing::GetGuideShadowOffsetPixels());
	BlendViewTransformCanvasDrawing::DrawDashedLine(
		Canvas,
		GuideStart + ShadowOffset,
		GuideEnd + ShadowOffset,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.55f),
		BlendViewTransformCanvasDrawing::GetGuideShadowThicknessPixels());
	BlendViewTransformCanvasDrawing::DrawDashedLine(
		Canvas,
		GuideStart,
		GuideEnd,
		FLinearColor::White,
		BlendViewTransformCanvasDrawing::GetGuideLineThicknessPixels());
}

void FBlendViewTransformTool::DrawAxisGuides(UCanvas* Canvas) const
{
	if (bMiddleMouseAxisSelection)
	{
		DrawAxisGuide(Canvas, EBlendViewAxisConstraint::X, IsAxisActiveForCurrentConstraint(EBlendViewAxisConstraint::X));
		DrawAxisGuide(Canvas, EBlendViewAxisConstraint::Y, IsAxisActiveForCurrentConstraint(EBlendViewAxisConstraint::Y));
		DrawAxisGuide(Canvas, EBlendViewAxisConstraint::Z, IsAxisActiveForCurrentConstraint(EBlendViewAxisConstraint::Z));
		return;
	}

	if (AxisConstraint != EBlendViewAxisConstraint::None)
	{
		if (AxisConstraint == EBlendViewAxisConstraint::PlaneX)
		{
			DrawAxisGuide(Canvas, EBlendViewAxisConstraint::Y, true);
			DrawAxisGuide(Canvas, EBlendViewAxisConstraint::Z, true);
		}
		else if (AxisConstraint == EBlendViewAxisConstraint::PlaneY)
		{
			DrawAxisGuide(Canvas, EBlendViewAxisConstraint::X, true);
			DrawAxisGuide(Canvas, EBlendViewAxisConstraint::Z, true);
		}
		else if (AxisConstraint == EBlendViewAxisConstraint::PlaneZ)
		{
			DrawAxisGuide(Canvas, EBlendViewAxisConstraint::X, true);
			DrawAxisGuide(Canvas, EBlendViewAxisConstraint::Y, true);
		}
		else
		{
			DrawAxisGuide(Canvas, AxisConstraint, true);
		}
	}
}

void FBlendViewTransformTool::DrawAxisGuide(
	UCanvas* Canvas,
	const EBlendViewAxisConstraint Axis,
	const bool bIsActive) const
{
	FVector2D PivotCanvasPosition;
	FVector2D Direction;
	const double DirectionSign = bIsActive ? AxisConstraintSign : 1.0;
	if (!GetAxisCanvasDirection(Canvas, Axis, DirectionSign, PivotCanvasPosition, Direction))
	{
		return;
	}

	if (bMiddleMouseAxisSelection && !bIsActive)
	{
		FVector2D GuideStart;
		FVector2D GuideEnd;
		if (GetMiddleMouseGuideCanvasSegment(Canvas, LastMouseViewportPosition, GuideStart, GuideEnd))
		{
			const double GuideSide = FVector2D::DotProduct(GuideEnd - GuideStart, Direction);
			if (GuideSide < 0.0)
			{
				Direction *= -1.0;
			}
		}
	}

	const double Extent = FMath::Max(Canvas->SizeX, Canvas->SizeY) * 2.0;
	BlendViewTransformCanvasDrawing::DrawLine(
		Canvas,
		PivotCanvasPosition - Direction * Extent,
		PivotCanvasPosition + Direction * Extent,
		GetBlendViewAxisColor(Axis, bIsActive),
		BlendViewTransformOverlay::GetAxisLineThicknessPixels());
}

bool FBlendViewTransformTool::GetAxisCanvasDirection(
	UCanvas* Canvas,
	const EBlendViewAxisConstraint Axis,
	const double Sign,
	FVector2D& OutStart,
	FVector2D& OutDirection) const
{
	if (!Canvas)
	{
		return false;
	}

	const FVector WorldAxis = GetAxisBaseVector(Axis);
	if (WorldAxis.IsNearlyZero())
	{
		return false;
	}

	FVector2D ProbeCanvasPosition;
	const double DirectionSign = Sign < 0.0 ? -1.0 : 1.0;
	const double ProjectionDistance = GetAxisProjectionWorldDistance();
	if (!ProjectWorldToCanvas(Canvas, OriginalPivotLocation, OutStart))
	{
		return false;
	}

	if (!ProjectWorldToCanvas(Canvas, OriginalPivotLocation + WorldAxis * DirectionSign * ProjectionDistance, ProbeCanvasPosition))
	{
		if (!ProjectWorldToCanvas(Canvas, OriginalPivotLocation - WorldAxis * DirectionSign * ProjectionDistance, ProbeCanvasPosition))
		{
			return false;
		}
		OutDirection = OutStart - ProbeCanvasPosition;
	}
	else
	{
		OutDirection = ProbeCanvasPosition - OutStart;
	}
	if (OutDirection.SizeSquared() < 1.0)
	{
		return false;
	}

	OutDirection.Normalize();
	return true;
}

void FBlendViewTransformTool::ApplyTranslationDelta(const FVector& Delta)
{
	ApplyTranslationDeltaInternal(Delta, false);
}

bool FBlendViewTransformTool::ApplyTranslationDeltaInternal(
	const FVector& Delta,
	const bool bFromOriginal)
{
	bool bMovedAny = false;
	const FVector SnapSourceBase = GetTranslationSnapSourceBase(bFromOriginal);
	FVector FinalDelta = SnapSession.bActiveForCurrentTranslation
		? ApplySnapToTranslationDelta(Delta, SnapSourceBase, bFromOriginal)
		: ApplyTranslationConstraint(Delta);
	if (!SnapSession.bActiveForCurrentTranslation)
	{
		FinalDelta = ApplyTranslationConstraint(
			ApplyUnrealTranslationSnapToDelta(FinalDelta));
	}
	FinalDelta = FBlendViewTransformPrecision::CleanNearInteger(FinalDelta);
	if (TargetAdapter.IsModelingPivotActive())
	{
		const bool bApplied = ApplyModelingPivotTranslationDelta(FinalDelta);
		AppliedTranslationDelta = CurrentPivotLocation - OriginalPivotLocation;
		return bApplied;
	}
	if (PivotEditSession.IsModelingPivotMode())
	{
		return false;
	}
	if (TargetAdapter.IsNativeComponentActive())
	{
		const bool bApplied = ApplyNativeComponentTranslationDelta(FinalDelta);
		AppliedTranslationDelta = CurrentPivotLocation - OriginalPivotLocation;
		return bApplied;
	}
	if (PivotEditSession.IsActorPivotMode())
	{
		const bool bApplied = ApplyActorPivotTranslationDelta(FinalDelta);
		AppliedTranslationDelta = CurrentPivotLocation - OriginalPivotLocation;
		return bApplied;
	}
	CurrentPivotLocation = FBlendViewTransformPrecision::CleanNearInteger(
		(bFromOriginal ? OriginalPivotLocation : InitialPivotLocation) + FinalDelta);
	AppliedTranslationDelta = CurrentPivotLocation - OriginalPivotLocation;
	for (const FBlendViewActorTransformSnapshot& Snapshot : TargetAdapter.GetActorSnapshots())
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		FTransform NewTransform = bFromOriginal ? Snapshot.InitialTransform : Snapshot.BaselineTransform;
		NewTransform.AddToTranslation(FinalDelta);
		ApplySnapTargetAlignment(NewTransform);
		NewTransform.SetLocation(FBlendViewTransformPrecision::CleanNearInteger(NewTransform.GetLocation()));
		Actor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		bMovedAny = true;
	}

	if (bMovedAny && GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}

	return bMovedAny;
}

bool FBlendViewTransformTool::ApplyTranslationDeltaFromOriginal(const FVector& Delta)
{
	return ApplyTranslationDeltaInternal(Delta, true);
}

bool FBlendViewTransformTool::RefreshTranslationFromCurrentIntent(const bool bSnapActive)
{
	if (Mode != EBlendViewTransformMode::Translate)
	{
		return false;
	}

	TGuardValue<bool> SnapGuard(SnapSession.bActiveForCurrentTranslation, bSnapActive);
	if (!bSnapActive)
	{
		ClearActiveSnapState();
	}

	if (bMiddleMouseAxisSelection)
	{
		if (IsSingleAxisConstraint(AxisConstraint))
		{
			return ProjectFreeMoveIntentToAxis(AxisConstraint, AxisConstraintSign, bLocalConstraint);
		}
		if (IsPlaneConstraint(AxisConstraint))
		{
			return UpdatePlaneTranslationConstraint(LastMouseViewportPosition);
		}
	}

	if (IsSingleAxisConstraint(AxisConstraint))
	{
		return ProjectFreeMoveIntentToAxis(AxisConstraint, AxisConstraintSign, bLocalConstraint);
	}
	if (IsPlaneConstraint(AxisConstraint))
	{
		return UpdatePlaneTranslationConstraint(LastMouseViewportPosition);
	}

	ApplyTranslationDelta(FreeMoveIntentPivotLocation - BaselineFreeMoveIntentPivotLocation);
	return true;
}

FVector FBlendViewTransformTool::ApplySnapToTranslationDelta(
	const FVector& Delta,
	const FVector& SnapSourceBase,
	const bool bFromOriginal)
{
	const FVector ConstrainedDelta = ApplyTranslationConstraint(Delta);
	const FVector SourceAfterDelta = SnapSourceBase + ConstrainedDelta;

	FBlendViewSnapCandidate Candidate;
	FBlendViewSnapQuery Query;
	Query.ViewportPosition = LastMouseViewportPosition;
	Query.SourceAfterDelta = SourceAfterDelta;
	Query.ViewportClient = InitialContext.ViewportClient;
	Query.GridSizeUnrealUnits = FBlendViewSnapSolver::GetTemporaryTranslationGridSize(ShouldUseUnrealEditorSnap());
	Query.ScreenRadiusScale = GetTransformSnapRadiusScale();
	ApplySnapTargetFilters(Query, true, true);
	FVector PointerTargetHint = SourceAfterDelta;
	if (GetMousePlaneIntersection(LastMouseViewportPosition, PointerTargetHint))
	{
		Query.TargetHintLocation = PointerTargetHint;
		Query.bHasTargetHintLocation = true;
	}
	Query.GetViewportTraceSegment = [this](
		const FVector2D& ViewportPosition,
		FVector& OutTraceStart,
		FVector& OutTraceEnd)
	{
		return GetViewportTraceSegment(ViewportPosition, OutTraceStart, OutTraceEnd);
	};
	Query.ProjectWorldToViewport = [this](const FVector& WorldPosition, FVector2D& OutViewportPosition)
	{
		return ProjectWorldToViewport(WorldPosition, OutViewportPosition);
	};
	if (!PivotEditSession.IsActorPivotMode())
	{
		Query.IgnoredActors.Reserve(TargetAdapter.GetActorSnapshots().Num());
		for (const FBlendViewActorTransformSnapshot& Snapshot : TargetAdapter.GetActorSnapshots())
		{
			Query.IgnoredActors.Add(Snapshot.Actor);
		}
		Query.IgnoredComponents.Reserve(TargetAdapter.GetComponentSnapshots().Num());
		for (const FBlendViewComponentTransformSnapshot& Snapshot : TargetAdapter.GetComponentSnapshots())
		{
			if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Snapshot.Component.Get()))
			{
				Query.IgnoredComponents.Add(PrimitiveComponent);
			}
			if (const UChildActorComponent* ChildActorComponent =
				Cast<UChildActorComponent>(Snapshot.Component.Get()))
			{
				if (AActor* ChildActor = ChildActorComponent->GetChildActor())
				{
					Query.IgnoredActors.Add(ChildActor);
				}
			}
		}
	}

	if (!SnapSolver.FindTemporarySnapTarget(Query, Candidate))
	{
		SnapSession.ClearActiveTarget();
		return ConstrainedDelta;
	}

	SnapSession.SetActiveTarget(
		Candidate.Location,
		Candidate.Normal,
		Candidate.bHasNormal,
		Candidate.Kind);
	const FVector ResolvedSnapSourceBase =
		ResolveTranslationSnapSourceBase(SnapSourceBase, ConstrainedDelta, bFromOriginal, Candidate);
	const FVector SnappedDelta = Candidate.Location - ResolvedSnapSourceBase;
	return ApplyTranslationConstraint(SnappedDelta);
}

FVector FBlendViewTransformTool::GetTranslationSnapSourceBase(const bool bFromOriginal) const
{
	if (SnapSession.bHasBase)
	{
		return SnapSession.BaseLocation;
	}

	if (TargetAdapter.IsModelingPivotActive() || PivotEditSession.IsAnyPivotMode())
	{
		return bFromOriginal ? OriginalPivotLocation : InitialPivotLocation;
	}

	return bFromOriginal
		? OriginalTranslationSnapSourceLocation
		: InitialTranslationSnapSourceLocation;
}

bool FBlendViewTransformTool::ShouldAlignRotationToSnapTarget() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings &&
		Settings->bAlignRotationToSnapTarget &&
		SnapSession.bActiveForCurrentTranslation &&
		SnapSession.bHasActiveTargetNormal &&
		SnapSession.ActiveTargetKind != EBlendViewSnapTargetKind::Grid;
}

void FBlendViewTransformTool::ApplySnapTargetAlignment(FTransform& InOutTransform) const
{
	if (!ShouldAlignRotationToSnapTarget())
	{
		return;
	}

	const FVector TargetNormal = SnapSession.ActiveTargetNormal.GetSafeNormal();
	const FVector SourceUp = InOutTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
	if (TargetNormal.IsNearlyZero() || SourceUp.IsNearlyZero())
	{
		return;
	}

	const FQuat AlignmentDelta = FQuat::FindBetweenNormals(SourceUp, TargetNormal);
	InOutTransform.SetRotation((AlignmentDelta * InOutTransform.GetRotation()).GetNormalized());
}

void FBlendViewTransformTool::ApplySnapTargetFilters(
	FBlendViewSnapQuery& Query,
	const bool bAllowGrid,
	const bool bApplyStructuralLimit) const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings)
	{
		return;
	}

	Query.bEnableGrid = bAllowGrid && Settings->bSnapTargetGrid;
	Query.bEnableVertex = Settings->bSnapTargetVertex;
	Query.bEnableEdge = Settings->bSnapTargetEdge;
	Query.bEnableEdgeMidpoint = Settings->bSnapTargetEdgeMidpoint;
	Query.bEnableFace = Settings->bSnapTargetFace;
	Query.bEnableStructuralEdgeLimit = bApplyStructuralLimit && Settings->bEnableStructuralEdgeLimit;
	Query.bEnableGeometry =
		Query.bEnableVertex ||
		Query.bEnableEdge ||
		Query.bEnableEdgeMidpoint ||
		Query.bEnableFace;
}

void FBlendViewTransformTool::ClearActiveSnapState()
{
	SnapSolver.Reset();
	SnapSession.ClearActiveTarget();
}

FVector FBlendViewTransformTool::ResolveTranslationSnapSourceBase(
	const FVector& PreferredSnapSourceBase,
	const FVector& Delta,
	const bool bFromOriginal,
	const FBlendViewSnapCandidate& SnapTarget) const
{
	if (SnapSession.bHasBase)
	{
		return PreferredSnapSourceBase;
	}

	if (PivotEditSession.IsActorPivotMode())
	{
		return PreferredSnapSourceBase;
	}

	if (!ShouldUseClosestTranslationSnapSource(SnapTarget))
	{
		return PreferredSnapSourceBase;
	}

	FVector ClosestSourceBase = FVector::ZeroVector;
	return TryResolveClosestTranslationSnapSourceBase(
		Delta,
		bFromOriginal,
		SnapTarget.Location,
		ClosestSourceBase)
		? ClosestSourceBase
		: PreferredSnapSourceBase;
}

bool FBlendViewTransformTool::ShouldUseClosestTranslationSnapSource(
	const FBlendViewSnapCandidate& SnapTarget) const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings &&
		Settings->SnapSourceMode == EBlendViewSnapSourceMode::Closest &&
		SnapTarget.Kind != EBlendViewSnapTargetKind::Grid;
}

bool FBlendViewTransformTool::TryResolveClosestTranslationSnapSourceBase(
	const FVector& Delta,
	const bool bFromOriginal,
	const FVector& SnapTargetLocation,
	FVector& OutSnapSourceBase) const
{
	if (TargetAdapter.IsNativeComponentActive() &&
		GetClosestNativeComponentVertexSnapSourceBase(
			Delta,
			bFromOriginal,
			SnapTargetLocation,
			OutSnapSourceBase))
	{
		return true;
	}

	return GetClosestSelectedVertexSnapSourceBase(
		Delta,
		bFromOriginal,
		SnapTargetLocation,
		OutSnapSourceBase);
}

bool FBlendViewTransformTool::GetClosestSelectedVertexSnapSourceBase(
	const FVector& Delta,
	const bool bFromOriginal,
	const FVector& SnapTargetLocation,
	FVector& OutSnapSourceBase) const
{
	if (!InitialContext.IsValid())
	{
		return false;
	}

	bool bHasBestSource = false;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (const FBlendViewActorTransformSnapshot& Snapshot : TargetAdapter.GetActorSnapshots())
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
		if (StaticMeshComponents.IsEmpty())
		{
			continue;
		}

		const FTransform& ActorSourceTransform = bFromOriginal ? Snapshot.InitialTransform : Snapshot.BaselineTransform;
		const FTransform CurrentActorTransform = Actor->GetActorTransform();
		for (const UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
		{
			if (!IsValid(StaticMeshComponent))
			{
				continue;
			}

			const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
			if (!StaticMesh || !StaticMesh->HasValidRenderData())
			{
				continue;
			}

			const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
			if (!RenderData || RenderData->LODResources.IsEmpty())
			{
				continue;
			}

			const FTransform ComponentRelativeToActor =
				StaticMeshComponent->GetComponentTransform().GetRelativeTransform(CurrentActorTransform);
			const FTransform ComponentSourceTransform = ComponentRelativeToActor * ActorSourceTransform;
			const FPositionVertexBuffer& Vertices = RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
			for (uint32 VertexIndex = 0; VertexIndex < Vertices.GetNumVertices(); ++VertexIndex)
			{
				const FVector SourceBase = ComponentSourceTransform.TransformPosition(FVector(Vertices.VertexPosition(VertexIndex)));
				const double DistanceSquared = FVector::DistSquared(SourceBase + Delta, SnapTargetLocation);
				if (!bHasBestSource || DistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = DistanceSquared;
					OutSnapSourceBase = SourceBase;
					bHasBestSource = true;
				}
			}
		}
	}

	return bHasBestSource;
}

bool FBlendViewTransformTool::GetClosestNativeComponentVertexSnapSourceBase(
	const FVector& Delta,
	const bool bFromOriginal,
	const FVector& SnapTargetLocation,
	FVector& OutSnapSourceBase) const
{
	bool bHasBestSource = false;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (const FBlendViewComponentTransformSnapshot& Snapshot : TargetAdapter.GetComponentSnapshots())
	{
		const USceneComponent* SourceComponent = Snapshot.Component.Get();
		if (!IsValid(SourceComponent))
		{
			continue;
		}

		auto EvaluateStaticMesh = [
			&Delta,
			bFromOriginal,
			&SnapTargetLocation,
			&OutSnapSourceBase,
			&bHasBestSource,
			&BestDistanceSquared,
			&Snapshot](
				const UStaticMeshComponent* StaticMeshComponent,
				const FTransform& MeshRelativeToSource)
		{
			if (!IsValid(StaticMeshComponent))
			{
				return;
			}

			const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
			if (!StaticMesh || !StaticMesh->HasValidRenderData())
			{
				return;
			}

			const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
			if (!RenderData || RenderData->LODResources.IsEmpty())
			{
				return;
			}

			const FTransform& SourceTransform = bFromOriginal
				? Snapshot.InitialTransform
				: Snapshot.BaselineTransform;
			const FTransform MeshSourceTransform = MeshRelativeToSource * SourceTransform;
			const FPositionVertexBuffer& Vertices =
				RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
			for (uint32 VertexIndex = 0; VertexIndex < Vertices.GetNumVertices(); ++VertexIndex)
			{
				const FVector SourceBase = MeshSourceTransform.TransformPosition(
					FVector(Vertices.VertexPosition(VertexIndex)));
				const double DistanceSquared =
					FVector::DistSquared(SourceBase + Delta, SnapTargetLocation);
				if (!bHasBestSource || DistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = DistanceSquared;
					OutSnapSourceBase = SourceBase;
					bHasBestSource = true;
				}
			}
		};

		if (const UStaticMeshComponent* StaticMeshComponent =
			Cast<UStaticMeshComponent>(SourceComponent))
		{
			EvaluateStaticMesh(StaticMeshComponent, FTransform::Identity);
			continue;
		}

		const UChildActorComponent* ChildActorComponent =
			Cast<UChildActorComponent>(SourceComponent);
		if (!IsValid(ChildActorComponent))
		{
			continue;
		}

		AActor* ChildActor = ChildActorComponent->GetChildActor();
		if (!IsValid(ChildActor))
		{
			continue;
		}

		TArray<UStaticMeshComponent*> ChildStaticMeshComponents;
		ChildActor->GetComponents<UStaticMeshComponent>(ChildStaticMeshComponents);
		const FTransform CurrentSourceTransform = ChildActorComponent->GetComponentTransform();
		for (const UStaticMeshComponent* StaticMeshComponent : ChildStaticMeshComponents)
		{
			if (!IsValid(StaticMeshComponent))
			{
				continue;
			}
			const FTransform MeshRelativeToSource =
				StaticMeshComponent->GetComponentTransform().GetRelativeTransform(
					CurrentSourceTransform);
			EvaluateStaticMesh(StaticMeshComponent, MeshRelativeToSource);
		}
	}

	return bHasBestSource;
}

FVector FBlendViewTransformTool::ApplyGridSnapToTranslationDelta(
	const FVector& Delta,
	const double GridSize) const
{
	return FBlendViewTransformSnapPolicy::SnapTranslationDelta(
		Delta,
		GridSize,
		AxisConstraint,
		GetConstraintSpaceRotation());
}

FVector FBlendViewTransformTool::ApplyUnrealTranslationSnapToDelta(const FVector& Delta) const
{
	if (!ShouldUseUnrealTranslationSnap())
	{
		return Delta;
	}

	const FBlendViewEditorSnapSettings SnapSettings = FBlendViewSnapSolver::GetEditorSnapSettings();
	return ApplyGridSnapToTranslationDelta(Delta, SnapSettings.LocationGridSize);
}

bool FBlendViewTransformTool::ShouldUseTranslationSnap(const bool bControlDown) const
{
	return bControlDown || FBlendViewSessionState::IsPersistentSnapEnabled() || SnapSession.bHasBase;
}

bool FBlendViewTransformTool::ShouldUseIncrementSnap(const bool bControlDown) const
{
	return (bControlDown || FBlendViewSessionState::IsPersistentSnapEnabled()) && !NumericInput.IsActive();
}

double FBlendViewTransformTool::GetTransformSnapRadiusScale() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return 1.0;
	}

	double Scale = static_cast<double>(BlendViewTransformOverlay::GetDPIScale());
	if (const TSharedPtr<SLevelViewport> LevelViewport = OverlaySession.GetLevelViewport())
	{
		if (const TSharedPtr<SWindow> Window =
				FSlateApplication::Get().FindWidgetWindow(LevelViewport.ToSharedRef()))
		{
			Scale *= FMath::Max(
				static_cast<double>(Window->GetDPIScaleFactor()),
				static_cast<double>(UE_SMALL_NUMBER));
		}
	}
	return Scale;
}

bool FBlendViewTransformTool::ShouldUseUnrealEditorSnap() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->bEnableUnrealEditorSnapCapture && !NumericInput.IsActive();
}

bool FBlendViewTransformTool::ShouldUseUnrealTranslationSnap() const
{
	if (!ShouldUseUnrealEditorSnap())
	{
		return false;
	}

	return FBlendViewSnapSolver::GetEditorSnapSettings().bLocationEnabled;
}

bool FBlendViewTransformTool::ShouldUseUnrealRotationSnap() const
{
	if (!ShouldUseUnrealEditorSnap())
	{
		return false;
	}

	return FBlendViewSnapSolver::GetEditorSnapSettings().bRotationEnabled;
}

bool FBlendViewTransformTool::ShouldUseUnrealScaleSnap() const
{
	if (!ShouldUseUnrealEditorSnap())
	{
		return false;
	}

	return FBlendViewSnapSolver::GetEditorSnapSettings().bScaleEnabled;
}

double FBlendViewTransformTool::SnapRotationAngleRadians(const double AngleRadians) const
{
	return FBlendViewTransformSnapPolicy::SnapRotationRadians(
		AngleRadians,
		bIncrementSnapActiveForCurrentTransform,
		ShouldUseUnrealRotationSnap(),
		AxisConstraint,
		FBlendViewSnapSolver::GetEditorSnapSettings().RotationGridSize);
}

double FBlendViewTransformTool::SnapScaleFactor(const double ScaleFactor) const
{
	return FBlendViewTransformSnapPolicy::SnapScaleFactor(
		ScaleFactor,
		bIncrementSnapActiveForCurrentTransform,
		bPrecisionModeActiveForCurrentTransform,
		NumericInput.IsActive(),
		ShouldUseUnrealScaleSnap(),
		FBlendViewSnapSolver::GetEditorSnapSettings().ScaleGridSize,
		GEditor && GEditor->UsePercentageBasedScaling());
}

FVector FBlendViewTransformTool::GetScaleFactorVector(const double AppliedScaleFactor) const
{
	return FBlendViewTransformSnapPolicy::BuildScaleFactorVector(
		AppliedScaleFactor,
		AxisConstraint);
}

bool FBlendViewTransformTool::ProjectFreeMoveIntentToAxis(
	const EBlendViewAxisConstraint Axis,
	const double PreferredSign,
	const bool bUseLocalConstraint)
{
	if (Mode != EBlendViewTransformMode::Translate)
	{
		return false;
	}

	SetAxisConstraint(Axis, PreferredSign, bUseLocalConstraint);
	const FVector AxisVector = GetAxisBaseVector(Axis);
	if (AxisVector.IsNearlyZero())
	{
		return false;
	}

	FVector2D GuideViewportPosition;
	if (!ProjectWorldToViewport(FreeMoveIntentPivotLocation, GuideViewportPosition))
	{
		return false;
	}

	double WorldDistance = 0.0;
	double AxisSign = PreferredSign;
	double Alignment = -1.0;
	if (!ProjectViewportGuideToAxis(Axis, GuideViewportPosition, WorldDistance, AxisSign, Alignment))
	{
		return false;
	}

	const FVector Delta = AxisVector * WorldDistance;
	SetAxisConstraint(Axis, AxisSign, bUseLocalConstraint);

	const bool bMovedAny = ApplyTranslationDeltaFromOriginal(Delta);
	return bMovedAny;
}

void FBlendViewTransformTool::ApplyRotationDelta(const double AngleRadians)
{
	const double AppliedAngleRadians = FMath::DegreesToRadians(
		FBlendViewTransformPrecision::CleanNearInteger(
			FMath::RadiansToDegrees(SnapRotationAngleRadians(AngleRadians))));
	AppliedRotationRadians = AppliedAngleRadians;
	FVector Axis = BlendViewViewportCompat::GetForwardVector(*InitialContext.ViewportClient);
	if (IsSingleAxisConstraint(AxisConstraint))
	{
		Axis = GetAxisBaseVector(AxisConstraint);
	}
	else if (IsPlaneConstraint(AxisConstraint))
	{
		Axis = GetAxisBaseVector(GetPlaneExcludedAxis(AxisConstraint));
	}
	const FQuat DeltaRotation(Axis, AppliedAngleRadians);
	if (TargetAdapter.IsModelingPivotActive())
	{
		ApplyModelingPivotRotationDelta(DeltaRotation);
		return;
	}
	if (PivotEditSession.IsModelingPivotMode())
	{
		return;
	}
	if (TargetAdapter.IsNativeComponentActive())
	{
		ApplyNativeComponentRotationDelta(DeltaRotation);
		return;
	}
	bool bChangedAny = false;
	CurrentPivotLocation = InitialPivotLocation;

	for (const FBlendViewActorTransformSnapshot& Snapshot : TargetAdapter.GetActorSnapshots())
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		const FTransform NewTransform = FBlendViewTransformMath::BuildRotatedTransform(
			Snapshot.BaselineTransform,
			bUseIndividualOrigins
				? GetActorSnapshotPivotWorldLocation(Snapshot.BaselineTransform, Snapshot.BaselinePivotOffset)
				: InitialPivotLocation,
			DeltaRotation);
		Actor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (USceneComponent* RootComponent = Actor->GetRootComponent())
		{
			RootComponent->SetRelativeRotationExact(
				FBlendViewTransformPrecision::CleanNearInteger(RootComponent->GetRelativeRotation()),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		bChangedAny = true;
	}

	if (bChangedAny && GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

void FBlendViewTransformTool::ApplyFreeRotationDelta(const FQuat& DeltaRotation)
{
	if (!DeltaRotation.IsNormalized())
	{
		return;
	}
	const double CleanAngleRadians = FMath::DegreesToRadians(
		FBlendViewTransformPrecision::CleanNearInteger(
			FMath::RadiansToDegrees(DeltaRotation.GetAngle())));
	const FQuat CleanDeltaRotation(DeltaRotation.GetRotationAxis(), CleanAngleRadians);
	AppliedRotationRadians = CleanAngleRadians;

	if (TargetAdapter.IsModelingPivotActive())
	{
		ApplyModelingPivotRotationDelta(CleanDeltaRotation);
		return;
	}
	if (PivotEditSession.IsModelingPivotMode())
	{
		return;
	}

	if (TargetAdapter.IsNativeComponentActive())
	{
		ApplyNativeComponentRotationDelta(CleanDeltaRotation);
		return;
	}

	bool bChangedAny = false;
	CurrentPivotLocation = InitialPivotLocation;
	for (const FBlendViewActorTransformSnapshot& Snapshot : TargetAdapter.GetActorSnapshots())
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		const FTransform NewTransform = FBlendViewTransformMath::BuildRotatedTransform(
			Snapshot.BaselineTransform,
			bUseIndividualOrigins
				? GetActorSnapshotPivotWorldLocation(Snapshot.BaselineTransform, Snapshot.BaselinePivotOffset)
				: InitialPivotLocation,
			CleanDeltaRotation);
		Actor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (USceneComponent* RootComponent = Actor->GetRootComponent())
		{
			RootComponent->SetRelativeRotationExact(
				FBlendViewTransformPrecision::CleanNearInteger(RootComponent->GetRelativeRotation()),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		bChangedAny = true;
	}

	if (bChangedAny && GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

void FBlendViewTransformTool::ApplyScaleFactor(const double ScaleFactor)
{
	const double AppliedScaleFactor = FBlendViewTransformPrecision::CleanNearInteger(
		SnapScaleFactor(ScaleFactor));
	ApplyScaleFactorVector(GetScaleFactorVector(AppliedScaleFactor));
}

void FBlendViewTransformTool::ApplyScaleFactorVector(const FVector& ScaleFactorVector)
{
	AppliedScaleFactors = ScaleFactorVector;
	if (TargetAdapter.IsModelingPivotActive() || PivotEditSession.IsModelingPivotMode())
	{
		return;
	}
	if (TargetAdapter.IsNativeComponentActive())
	{
		TargetAdapter.ApplyNativeComponentScaleFactor(
			ScaleFactorVector,
			InitialPivotLocation,
			LocalConstraintRotation,
			bLocalConstraint,
			bUseIndividualOrigins,
			CurrentPivotLocation);
		return;
	}
	bool bChangedAny = false;
	CurrentPivotLocation = InitialPivotLocation;
	for (const FBlendViewActorTransformSnapshot& Snapshot : TargetAdapter.GetActorSnapshots())
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		const FTransform NewTransform = FBlendViewTransformMath::BuildScaledTransform(
			Snapshot.BaselineTransform,
			bUseIndividualOrigins
				? GetActorSnapshotPivotWorldLocation(Snapshot.BaselineTransform, Snapshot.BaselinePivotOffset)
				: InitialPivotLocation,
			ScaleFactorVector,
			LocalConstraintRotation,
			bLocalConstraint);
		if (!NewTransform.ContainsNaN())
		{
			Actor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
			bChangedAny = true;
		}
	}

	if (bChangedAny && GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

FVector FBlendViewTransformTool::GetMirrorScaleFactorVector() const
{
	switch (AxisConstraint)
	{
	case EBlendViewAxisConstraint::X:
		return FVector(-1.0, 1.0, 1.0);
	case EBlendViewAxisConstraint::Y:
		return FVector(1.0, -1.0, 1.0);
	case EBlendViewAxisConstraint::Z:
		return FVector(1.0, 1.0, -1.0);
	case EBlendViewAxisConstraint::PlaneX:
		return FVector(1.0, -1.0, -1.0);
	case EBlendViewAxisConstraint::PlaneY:
		return FVector(-1.0, 1.0, -1.0);
	case EBlendViewAxisConstraint::PlaneZ:
		return FVector(-1.0, -1.0, 1.0);
	default:
		return FVector::OneVector;
	}
}

void FBlendViewTransformTool::ApplyMirrorTransform()
{
	if (AxisConstraint == EBlendViewAxisConstraint::None)
	{
		RestoreOperationBaselineToOriginal();
		AppliedScaleFactors = FVector::OneVector;
		return;
	}

	const FVector MirrorScaleFactor = GetMirrorScaleFactorVector();
	AppliedScaleFactors = MirrorScaleFactor;
	if (TargetAdapter.IsNativeComponentActive())
	{
		TargetAdapter.ApplyNativeComponentMirrorTransform(
			MirrorScaleFactor,
			InitialPivotLocation,
			LocalConstraintRotation,
			bLocalConstraint,
			bUseIndividualOrigins,
			CurrentPivotLocation);
		return;
	}

	bool bChangedAny = false;
	CurrentPivotLocation = InitialPivotLocation;
	for (const FBlendViewActorTransformSnapshot& Snapshot : TargetAdapter.GetActorSnapshots())
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		const FTransform NewTransform = FBlendViewTransformMath::BuildMirroredTransform(
			Snapshot.BaselineTransform,
			bUseIndividualOrigins
				? GetActorSnapshotPivotWorldLocation(Snapshot.BaselineTransform, Snapshot.BaselinePivotOffset)
				: InitialPivotLocation,
			MirrorScaleFactor,
			LocalConstraintRotation,
			bLocalConstraint);
		if (!NewTransform.ContainsNaN())
		{
			Actor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
			bChangedAny = true;
		}
	}

	if (bChangedAny && GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FBlendViewTransformTool::ApplyNativeComponentTranslationDelta(const FVector& Delta)
{
	return TargetAdapter.ApplyNativeComponentTranslationDelta(Delta, InitialPivotLocation, CurrentPivotLocation);
}

bool FBlendViewTransformTool::ApplyModelingPivotTranslationDelta(const FVector& Delta)
{
	return TargetAdapter.ApplyModelingPivotTranslationDelta(Delta, InitialPivotLocation, CurrentPivotLocation);
}

bool FBlendViewTransformTool::ApplyActorPivotTranslationDelta(const FVector& Delta)
{
	return TargetAdapter.ApplyActorPivotTranslationDelta(Delta, InitialPivotLocation, CurrentPivotLocation);
}

bool FBlendViewTransformTool::ApplyNativeComponentRotationDelta(const FQuat& DeltaRotation)
{
	return TargetAdapter.ApplyNativeComponentRotationDelta(
		DeltaRotation,
		InitialPivotLocation,
		bUseIndividualOrigins,
		CurrentPivotLocation);
}

bool FBlendViewTransformTool::ApplyModelingPivotRotationDelta(const FQuat& DeltaRotation)
{
	return TargetAdapter.ApplyModelingPivotRotationDelta(
		DeltaRotation,
		InitialPivotLocation,
		CurrentPivotLocation);
}

bool FBlendViewTransformTool::ResetModelingPivotTransformComponent(
	const EBlendViewTransformResetChannel Channel)
{
	if (!PivotEditSession.IsModelingPivotMode() || !TargetAdapter.IsModelingPivotActive())
	{
		return false;
	}

	bool bReset = false;
	switch (Channel)
	{
	case EBlendViewTransformResetChannel::Location:
		bReset = TargetAdapter.ResetModelingPivotLocation(CurrentPivotLocation, LocalConstraintRotation);
		AppliedTranslationDelta = CurrentPivotLocation - OriginalPivotLocation;
		break;
	case EBlendViewTransformResetChannel::Rotation:
		bReset = TargetAdapter.ResetModelingPivotRotation(CurrentPivotLocation, LocalConstraintRotation);
		AppliedRotationRadians = 0.0;
		break;
	default:
		return false;
	}

	if (!bReset)
	{
		return false;
	}

	InitialPivotLocation = CurrentPivotLocation;
	FreeMoveIntentPivotLocation = CurrentPivotLocation;
	BaselineFreeMoveIntentPivotLocation = CurrentPivotLocation;
	bFreeRotateMode = false;
	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	ClearActiveSnapState();
	PlaneTranslationSession.Reset();
	NumericInput.Clear();
	ResetInputBaselineForMode(Mode, LastMouseViewportPosition);
	UpdateViewportOverlay();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
	if (InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->Invalidate();
	}
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView reset Modeling Edit Pivot component: %s"),
		Channel == EBlendViewTransformResetChannel::Location ? TEXT("Location") : TEXT("Rotation"));
	return true;
}

void FBlendViewTransformTool::SetMode(const EBlendViewTransformMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	bFreeRotateMode = false;
	PivotEditSession.ExitActorPivotMode();
	if (bViewportWidgetHidden && InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->ShowWidget(false);
	}
	RestoreOperationBaselineToOriginal();
	SetAxisConstraint(EBlendViewAxisConstraint::None, 1.0);
	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	PlaneTranslationSession.Reset();
	FreeRotateAccumulatedViewportDelta = FVector2D::ZeroVector;
	NumericInput.Clear();
	ResetInputBaselineForMode(InMode, LastMouseViewportPosition);
	RestoreTransformCursorOverride();
	ApplyTransformCursorOverride();
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform switch: %s"), GetModeName());
}

bool FBlendViewTransformTool::EnterModelingPivotEditMode()
{
	if (Mode != EBlendViewTransformMode::Translate ||
		TargetAdapter.IsNativeComponentActive() ||
		PivotEditSession.IsModelingPivotMode())
	{
		return false;
	}

	RestoreOperationBaselineToOriginal();
	TargetAdapter.CancelTransaction();
	if (!TargetAdapter.CaptureModelingPivot(
			InitialContext,
			OriginalPivotLocation,
			LocalConstraintRotation))
	{
		UE_LOG(LogBlendViewTransform, Warning, TEXT("BlendView GOO failed: Modeling Edit Pivot tool is unavailable"));
		bCompleteRequested = true;
		return false;
	}

	if (bViewportWidgetHidden && InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->ShowWidget(false);
	}
	if (InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->Invalidate();
	}

	bCompleteRequested = true;
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView Modeling Edit Pivot tool entered"));
	return true;
}

void FBlendViewTransformTool::TogglePivotEditMode()
{
	if (PivotEditSession.IsModelingPivotMode() ||
		Mode != EBlendViewTransformMode::Translate ||
		TargetAdapter.IsNativeComponentActive())
	{
		return;
	}

	if (PivotEditSession.IsActorPivotMode())
	{
		EnterModelingPivotEditMode();
		return;
	}

	const bool bWasPivotEditMode = PivotEditSession.IsActorPivotMode();
	if (bWasPivotEditMode)
	{
		PivotEditSession.ExitActorPivotMode();
	}
	else
	{
		PivotEditSession.EnterActorPivotMode();
	}
	if (bViewportWidgetHidden && InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->ShowWidget(PivotEditSession.IsActorPivotMode());
	}
	RestoreOperationBaselineToOriginal();
	SetAxisConstraint(EBlendViewAxisConstraint::None, 1.0);
	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	PlaneTranslationSession.Reset();
	NumericInput.Clear();
	ResetInputBaselineForMode(Mode, LastMouseViewportPosition);
	UpdateViewportOverlay();
	if (InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->Invalidate();
	}

	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView pivot edit mode: %s -> %s"),
		bWasPivotEditMode ? TEXT("on") : TEXT("off"),
		PivotEditSession.IsActorPivotMode() ? TEXT("on") : TEXT("off"));
}

void FBlendViewTransformTool::EnterFreeRotateMode()
{
	if (Mode != EBlendViewTransformMode::Rotate)
	{
		SetMode(EBlendViewTransformMode::Rotate);
	}

	bFreeRotateMode = true;
	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	PlaneTranslationSession.Reset();
	SetAxisConstraint(EBlendViewAxisConstraint::None, 1.0);
	NumericInput.Clear();
	ResetBaselineFromCurrent(LastMouseViewportPosition);
	UpdateViewportOverlay();
	if (InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->Invalidate();
	}
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform rotate free"));
}

void FBlendViewTransformTool::ToggleAxisConstraint(const EBlendViewAxisConstraint InAxis)
{
	bFreeRotateMode = false;
	const bool bUseLocalConstraint = AxisConstraint == InAxis ? !bLocalConstraint : false;

	if (Mode == EBlendViewTransformMode::Translate)
	{
		PlaneTranslationSession.Reset();
		ProjectFreeMoveIntentToAxis(InAxis, 1.0, bUseLocalConstraint);
	}
	else
	{
		RestoreOperationBaselineToOriginal();
		const bool bConstraintChanged = SetAxisConstraint(InAxis, 1.0, bUseLocalConstraint);
		ResetInputBaselineForMode(Mode, LastMouseViewportPosition);
		if (Mode == EBlendViewTransformMode::Mirror && bConstraintChanged)
		{
			ApplyMirrorTransform();
		}
	}
	if (NumericInput.IsActive())
	{
		ApplyNumericTransform();
	}
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform axis: %s"), GetAxisName());
}

void FBlendViewTransformTool::TogglePlaneConstraint(const EBlendViewAxisConstraint InPlane)
{
	if (!IsPlaneConstraint(InPlane))
	{
		return;
	}

	bFreeRotateMode = false;
	const bool bUseLocalConstraint = AxisConstraint == InPlane ? !bLocalConstraint : false;

	if (Mode == EBlendViewTransformMode::Translate)
	{
		BeginPlaneTranslationConstraint(InPlane, LastMouseViewportPosition, bUseLocalConstraint);
	}
	else
	{
		RestoreOperationBaselineToOriginal();
		const bool bConstraintChanged = SetAxisConstraint(InPlane, 1.0, bUseLocalConstraint);
		ResetInputBaselineForMode(Mode, LastMouseViewportPosition);
		if (Mode == EBlendViewTransformMode::Mirror && bConstraintChanged)
		{
			ApplyMirrorTransform();
		}
	}
	if (NumericInput.IsActive())
	{
		ApplyNumericTransform();
	}
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform axis: %s"), GetAxisName());
}

void FBlendViewTransformTool::ClearConstraint()
{
	if (AxisConstraint == EBlendViewAxisConstraint::None)
	{
		return;
	}

	const bool bNumericInputActive = NumericInput.IsActive();
	if (bNumericInputActive)
	{
		RestoreOperationBaselineToOriginal();
	}
	else
	{
		TargetAdapter.ResetBaselineFromCurrent();
		InitialTranslationSnapSourceLocation += CurrentPivotLocation - InitialPivotLocation;
		InitialPivotLocation = CurrentPivotLocation;
		FreeMoveIntentPivotLocation = CurrentPivotLocation;
		BaselineFreeMoveIntentPivotLocation = CurrentPivotLocation;
	}
	bFreeRotateMode = false;
	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	SetAxisConstraint(EBlendViewAxisConstraint::None, 1.0);
	ResetInputBaselineForMode(Mode, LastMouseViewportPosition);
	if (bNumericInputActive)
	{
		ApplyNumericTransform();
	}
	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView transform constraint cleared"));
}

void FBlendViewTransformTool::RestoreOperationBaselineToOriginal()
{
	TargetAdapter.RestoreInitialTransforms(CurrentPivotLocation, OriginalPivotLocation);
	InitialPivotLocation = OriginalPivotLocation;
	CurrentPivotLocation = OriginalPivotLocation;
	InitialTranslationSnapSourceLocation = OriginalTranslationSnapSourceLocation;
	FreeMoveIntentPivotLocation = OriginalPivotLocation;
	BaselineFreeMoveIntentPivotLocation = OriginalPivotLocation;
	ResetAppliedTransformValues();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FBlendViewTransformTool::BeginPlaneTranslationConstraint(
	const EBlendViewAxisConstraint Plane,
	const FVector2D& ViewportPosition,
	const bool bUseLocalConstraint)
{
	if (Mode != EBlendViewTransformMode::Translate || !IsPlaneConstraint(Plane))
	{
		return false;
	}

	const int32 ExcludedAxisIndex = GetPlaneExcludedAxisIndex(Plane);
	if (ExcludedAxisIndex == INDEX_NONE)
	{
		return false;
	}

	SetAxisConstraint(Plane, 1.0, bUseLocalConstraint);
	FVector PointerLocation;
	if (!GetPlanePointerLocation(
		Plane,
		ViewportPosition,
		bUseLocalConstraint,
		PointerLocation))
	{
		PlaneTranslationSession.Reset();
		return false;
	}

	const FQuat ConstraintRotation = bUseLocalConstraint
		? LocalConstraintRotation.GetNormalized()
		: FQuat::Identity;
	const FVector PlaneDelta = PlaneTranslationSession.Begin(
		PointerLocation,
		CurrentPivotLocation - OriginalPivotLocation,
		ConstraintRotation,
		ExcludedAxisIndex);
	ApplyTranslationDeltaFromOriginal(PlaneDelta);
	FreeMoveIntentPivotLocation = CurrentPivotLocation;
	return true;
}

bool FBlendViewTransformTool::UpdatePlaneTranslationConstraint(
	const FVector2D& ViewportPosition)
{
	if (Mode != EBlendViewTransformMode::Translate ||
		!IsPlaneConstraint(AxisConstraint))
	{
		return false;
	}

	const int32 ExcludedAxisIndex = GetPlaneExcludedAxisIndex(AxisConstraint);
	if (ExcludedAxisIndex == INDEX_NONE)
	{
		return false;
	}

	const FQuat ConstraintRotation = bLocalConstraint
		? LocalConstraintRotation.GetNormalized()
		: FQuat::Identity;
	FVector PointerLocation;
	if (!GetPlanePointerLocation(
		AxisConstraint,
		ViewportPosition,
		bLocalConstraint,
		PointerLocation))
	{
		return false;
	}

	if (!PlaneTranslationSession.Matches(ConstraintRotation, ExcludedAxisIndex))
	{
		PlaneTranslationSession.Begin(
			PointerLocation,
			CurrentPivotLocation - OriginalPivotLocation,
			ConstraintRotation,
			ExcludedAxisIndex);
		return true;
	}

	const bool bMovedAny = ApplyTranslationDeltaFromOriginal(
		PlaneTranslationSession.Solve(PointerLocation));
	if (bMovedAny)
	{
		FreeMoveIntentPivotLocation = CurrentPivotLocation;
	}
	return bMovedAny;
}

bool FBlendViewTransformTool::GetPlanePointerLocation(
	const EBlendViewAxisConstraint Plane,
	const FVector2D& ViewportPosition,
	const bool bUseLocalConstraint,
	FVector& OutPointerLocation) const
{
	const int32 ExcludedAxisIndex = GetPlaneExcludedAxisIndex(Plane);
	if (ExcludedAxisIndex == INDEX_NONE)
	{
		return false;
	}

	FVector PlaneNormal = FVector::ZeroVector;
	PlaneNormal[ExcludedAxisIndex] = 1.0;
	const FQuat ConstraintRotation = bUseLocalConstraint
		? LocalConstraintRotation.GetNormalized()
		: FQuat::Identity;
	PlaneNormal = ConstraintRotation.RotateVector(PlaneNormal).GetSafeNormal();

	FVector RayStart;
	FVector RayDirection;
	if (!GetViewportRay(ViewportPosition, RayStart, RayDirection) ||
		FMath::IsNearlyZero(FVector::DotProduct(RayDirection, PlaneNormal)))
	{
		return false;
	}

	OutPointerLocation = FMath::LinePlaneIntersection(
		RayStart,
		RayStart + RayDirection * 1000000.0,
		OriginalPivotLocation,
		PlaneNormal);
	return !OutPointerLocation.ContainsNaN();
}

int32 FBlendViewTransformTool::GetPlaneExcludedAxisIndex(
	const EBlendViewAxisConstraint Plane) const
{
	switch (GetPlaneExcludedAxis(Plane))
	{
	case EBlendViewAxisConstraint::X:
		return 0;
	case EBlendViewAxisConstraint::Y:
		return 1;
	case EBlendViewAxisConstraint::Z:
		return 2;
	default:
		return INDEX_NONE;
	}
}

void FBlendViewTransformTool::HandleNumericBackspace()
{
	const EBlendViewNumericBackspaceResult Result = NumericInput.HandleBackspace();
	if (Result == EBlendViewNumericBackspaceResult::Ignored)
	{
		return;
	}
	if (Result == EBlendViewNumericBackspaceResult::Cleared)
	{
		RestoreOperationBaselineToOriginal();
		ResetInputBaselineForMode(Mode, LastMouseViewportPosition);
		return;
	}

	ApplyNumericTransform();
}

void FBlendViewTransformTool::ApplyNumericTransform()
{
	if (!NumericInput.IsActive())
	{
		return;
	}

	const double Value = NumericInput.GetValue();
	RestoreOperationBaselineToOriginal();

	switch (Mode)
	{
	case EBlendViewTransformMode::Translate:
		ApplyTranslationDeltaFromOriginal(
			GetNumericTranslationDirection() *
			Value *
			GetDefault<UBlendViewSettings>()->GetTranslationNumericUnitScale());
		FreeMoveIntentPivotLocation = CurrentPivotLocation;
		break;
	case EBlendViewTransformMode::Rotate:
		ApplyRotationDelta(FMath::DegreesToRadians(Value));
		break;
	case EBlendViewTransformMode::Scale:
		ApplyScaleFactor(Value);
		break;
	default:
		break;
	}

	UE_LOG(LogBlendViewTransform, Verbose, TEXT("BlendView numeric transform: %s %s"),
		GetModeName(),
		*NumericInput.GetBuffer());
}

void FBlendViewTransformTool::ResetTransientModalState()
{
	SnapSession.ResetTransient();
	ClearActiveSnapState();
	bMiddleMouseAxisSelection = false;
	bMiddleMousePlaneSelection = false;
	bFreeRotateMode = false;
	PivotEditSession.Reset();
	bIncrementSnapActiveForCurrentTransform = false;
	bPrecisionModeActiveForCurrentTransform = false;
	PlaneTranslationSession.Reset();
	RotationAccumulator.Reset();
	NumericInput.Clear();
	FrozenTransformValueText.Reset();
	ResetAppliedTransformValues();
}

FVector FBlendViewTransformTool::GetNumericTranslationDirection() const
{
	if (IsSingleAxisConstraint(AxisConstraint))
	{
		return GetAxisBaseVector(AxisConstraint) * AxisConstraintSign;
	}

	FVector Direction = FreeMoveIntentPivotLocation - OriginalPivotLocation;
	if (IsPlaneConstraint(AxisConstraint))
	{
		Direction = FBlendViewTransformConstraintUtils::ProjectVectorOntoPlane(
			Direction,
			AxisConstraint,
			GetConstraintSpaceRotation());
	}

	if (!Direction.IsNearlyZero())
	{
		return Direction.GetSafeNormal();
	}

	Direction = InitialMouseWorldLocation - OriginalPivotLocation;
	if (IsPlaneConstraint(AxisConstraint))
	{
		Direction = FBlendViewTransformConstraintUtils::ProjectVectorOntoPlane(
			Direction,
			AxisConstraint,
			GetConstraintSpaceRotation());
	}

	return Direction.IsNearlyZero() ? FVector::XAxisVector : Direction.GetSafeNormal();
}

EBlendViewAxisConstraint FBlendViewTransformTool::AxisToPlaneConstraint(
	const EBlendViewAxisConstraint Axis) const
{
	return FBlendViewTransformConstraintUtils::AxisToPlane(Axis);
}

bool FBlendViewTransformTool::IsAxisActiveForCurrentConstraint(
	const EBlendViewAxisConstraint Axis) const
{
	return FBlendViewTransformConstraintUtils::IsAxisActive(AxisConstraint, Axis);
}

void FBlendViewTransformTool::ResetInputBaselineForMode(
	const EBlendViewTransformMode InMode,
	const FVector2D& ViewportPosition,
	const EScaleSpringResetPolicy ScaleSpringPolicy)
{
	InitialMouseViewportPosition = ViewportPosition;
	LastMouseViewportPosition = ViewportPosition;
	VirtualPointer.Reset(ViewportPosition);
	PlaneTranslationSession.Reset();
	bHasPivotViewportPosition = ProjectWorldToViewport(InitialPivotLocation, PivotViewportPosition);
	bHasMouseGuideWorldLocation = GetMousePlaneIntersection(InitialMouseViewportPosition, MouseGuideWorldLocation);
	bHasInitialMouseWorldLocation = bHasMouseGuideWorldLocation;
	if (bHasInitialMouseWorldLocation)
	{
		InitialMouseWorldLocation = MouseGuideWorldLocation;
	}
	RotationAccumulator.Reset();
	if (InMode == EBlendViewTransformMode::Rotate && bHasInitialMouseWorldLocation)
	{
		RotationAccumulator.Begin(InitialPivotLocation - InitialMouseWorldLocation);
	}

	if (InMode == EBlendViewTransformMode::Scale)
	{
		if (ScaleSpringPolicy == EScaleSpringResetPolicy::Reset)
		{
			ResetScaleSpringInput(ViewportPosition);
		}
	}
	else
	{
		ScaleSpring.Reset();
	}

}

void FBlendViewTransformTool::ResetBaselineFromCurrent(const FVector2D& ViewportPosition)
{
	TargetAdapter.ResetBaselineFromCurrent();
	ResetAppliedTransformValues();

	InitialTranslationSnapSourceLocation += CurrentPivotLocation - InitialPivotLocation;
	InitialPivotLocation = CurrentPivotLocation;
	FreeMoveIntentPivotLocation = CurrentPivotLocation;
	BaselineFreeMoveIntentPivotLocation = FreeMoveIntentPivotLocation;
	InitialMouseViewportPosition = ViewportPosition;
	LastMouseViewportPosition = ViewportPosition;
	VirtualPointer.Reset(ViewportPosition);
	PlaneTranslationSession.Reset();
	FreeRotateAccumulatedViewportDelta = FVector2D::ZeroVector;
}

FVector FBlendViewTransformTool::ApplyTranslationConstraint(const FVector& Delta) const
{
	if (IsPlaneConstraint(AxisConstraint))
	{
		const int32 ExcludedAxisIndex = GetPlaneExcludedAxisIndex(AxisConstraint);
		const FQuat ConstraintRotation = GetConstraintSpaceRotation();
		if (PlaneTranslationSession.Matches(ConstraintRotation, ExcludedAxisIndex))
		{
			return PlaneTranslationSession.ConstrainDelta(Delta);
		}

	}

	return FBlendViewTransformConstraintUtils::ApplyVectorConstraint(
		Delta,
		AxisConstraint,
		AxisConstraintSign,
		bLocalConstraint,
		LocalConstraintRotation);
}

bool FBlendViewTransformTool::IsSingleAxisConstraint(const EBlendViewAxisConstraint Constraint) const
{
	return FBlendViewTransformConstraintUtils::IsSingleAxis(Constraint);
}

bool FBlendViewTransformTool::IsPlaneConstraint(const EBlendViewAxisConstraint Constraint) const
{
	return FBlendViewTransformConstraintUtils::IsPlane(Constraint);
}

EBlendViewAxisConstraint FBlendViewTransformTool::GetPlaneExcludedAxis(
	const EBlendViewAxisConstraint Plane) const
{
	return FBlendViewTransformConstraintUtils::GetPlaneExcludedAxis(Plane);
}

FVector FBlendViewTransformTool::GetAxisBaseVector(const EBlendViewAxisConstraint Axis) const
{
	return FBlendViewTransformConstraintUtils::GetAxisVector(
		Axis,
		bLocalConstraint,
		LocalConstraintRotation);
}

FVector FBlendViewTransformTool::GetConstraintAxisVector() const
{
	return FBlendViewTransformConstraintUtils::GetConstraintAxisVector(
		AxisConstraint,
		AxisConstraintSign,
		bLocalConstraint,
		LocalConstraintRotation);
}

FQuat FBlendViewTransformTool::GetConstraintSpaceRotation() const
{
	return FBlendViewTransformConstraintUtils::GetSpaceRotation(
		bLocalConstraint,
		LocalConstraintRotation);
}

FVector FBlendViewTransformTool::GetRotationAxisVector() const
{
	if (IsSingleAxisConstraint(AxisConstraint))
	{
		return GetAxisBaseVector(AxisConstraint);
	}
	if (IsPlaneConstraint(AxisConstraint))
	{
		return GetAxisBaseVector(GetPlaneExcludedAxis(AxisConstraint));
	}

	if (!InitialContext.IsValid())
	{
		return FVector::ZeroVector;
	}
	if (!InitialContext.ViewportClient->IsPerspective())
	{
		return -BlendViewViewportCompat::GetForwardVector(*InitialContext.ViewportClient);
	}

	const FVector CameraToPivotAxis =
		(InitialContext.ViewportClient->GetViewLocation() - InitialPivotLocation).GetSafeNormal();
	if (!CameraToPivotAxis.IsNearlyZero())
	{
		return CameraToPivotAxis;
	}

	return -BlendViewViewportCompat::GetForwardVector(*InitialContext.ViewportClient);
}

FBlendViewStatusLine FBlendViewTransformTool::BuildStatusLine() const
{
	FBlendViewTransformStatusState State;
	State.Mode = Mode;
	State.Constraint = AxisConstraint;
	State.bSelectingSnapBase = SnapSession.bSelectingBase;
	State.bHasSnapBase = SnapSession.bHasBase;
	State.bFreeRotate = bFreeRotateMode;
	State.bPivotEditMode = PivotEditSession.IsActorPivotMode();
	State.bModelingPivotEditMode = PivotEditSession.IsModelingPivotMode();
	if (Mode == EBlendViewTransformMode::Mirror)
	{
		State.bSupportsTrackball = false;
		State.bSupportsSnapBase = false;
	}
	if (NumericInput.IsActive())
	{
		State.NumericBuffer = NumericInput.GetBuffer();
	}
	return FBlendViewTransformStatusBuilder::Build(State);
}

FText FBlendViewTransformTool::BuildTransformValueText() const
{
	FBlendViewTransformValueState State;
	State.Mode = Mode;
	State.Constraint = AxisConstraint;
	State.Translation = bLocalConstraint
		? LocalConstraintRotation.UnrotateVector(AppliedTranslationDelta)
		: AppliedTranslationDelta;
	State.RotationDegrees = FMath::RadiansToDegrees(AppliedRotationRadians);
	State.Scale = AppliedScaleFactors;
	State.bLocalConstraint = bLocalConstraint;
	State.bTrackball = Mode == EBlendViewTransformMode::Rotate && bFreeRotateMode;
	State.bPivotEditMode = PivotEditSession.IsAnyPivotMode();

	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	const EBlendViewTranslationNumericUnit Unit = Settings
		? Settings->TranslationNumericUnit
		: EBlendViewTranslationNumericUnit::Meters;
	const bool bChinese =
		FBlendViewLocalization::GetResolvedLanguage() == EBlendViewResolvedLanguage::Chinese;
	return FBlendViewTransformValueFormatter::Format(State, Unit, bChinese);
}

void FBlendViewTransformTool::ResetAppliedTransformValues()
{
	AppliedTranslationDelta = FVector::ZeroVector;
	AppliedRotationRadians = 0.0;
	AppliedScaleFactors = FVector::OneVector;
}

bool FBlendViewTransformTool::SetAxisConstraint(
	const EBlendViewAxisConstraint InAxis,
	const double InSign,
	const bool bInLocalConstraint)
{
	const bool bNormalizedLocalConstraint =
		InAxis != EBlendViewAxisConstraint::None && bInLocalConstraint;
	const double NormalizedSign = InSign < 0.0 ? -1.0 : 1.0;
	const bool bChanged =
		AxisConstraint != InAxis ||
		!FMath::IsNearlyEqual(AxisConstraintSign, NormalizedSign) ||
		bLocalConstraint != bNormalizedLocalConstraint;
	if (bChanged)
	{
		PlaneTranslationSession.Reset();
	}

	AxisConstraint = InAxis;
	AxisConstraintSign = NormalizedSign;
	bLocalConstraint = bNormalizedLocalConstraint;

	if (bChanged && InitialContext.ViewportClient)
	{
		InitialContext.ViewportClient->Invalidate();
	}
	return bChanged;
}

const TCHAR* FBlendViewTransformTool::GetModeName() const
{
	switch (Mode)
	{
	case EBlendViewTransformMode::Translate:
		return TEXT("Translate");
	case EBlendViewTransformMode::Rotate:
		return TEXT("Rotate");
	case EBlendViewTransformMode::Scale:
		return TEXT("Scale");
	case EBlendViewTransformMode::Mirror:
		return TEXT("Mirror");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* FBlendViewTransformTool::GetAxisName() const
{
	switch (AxisConstraint)
	{
	case EBlendViewAxisConstraint::X:
		if (bLocalConstraint) return AxisConstraintSign < 0.0 ? TEXT("Local -X") : TEXT("Local X");
		return AxisConstraintSign < 0.0 ? TEXT("-X") : TEXT("X");
	case EBlendViewAxisConstraint::Y:
		if (bLocalConstraint) return AxisConstraintSign < 0.0 ? TEXT("Local -Y") : TEXT("Local Y");
		return AxisConstraintSign < 0.0 ? TEXT("-Y") : TEXT("Y");
	case EBlendViewAxisConstraint::Z:
		if (bLocalConstraint) return AxisConstraintSign < 0.0 ? TEXT("Local -Z") : TEXT("Local Z");
		return AxisConstraintSign < 0.0 ? TEXT("-Z") : TEXT("Z");
	case EBlendViewAxisConstraint::PlaneX:
		return bLocalConstraint ? TEXT("Local YZ Plane") : TEXT("YZ Plane");
	case EBlendViewAxisConstraint::PlaneY:
		return bLocalConstraint ? TEXT("Local XZ Plane") : TEXT("XZ Plane");
	case EBlendViewAxisConstraint::PlaneZ:
		return bLocalConstraint ? TEXT("Local XY Plane") : TEXT("XY Plane");
	default:
		return TEXT("None");
	}
}
