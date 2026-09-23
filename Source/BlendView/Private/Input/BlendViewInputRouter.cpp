// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Input/BlendViewInputRouter.h"

#include "BlendViewCommands.h"
#include "BlendViewSettings.h"
#include "Compat/BlendViewEngineVersion.h"
#include "Core/BlendViewSessionState.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Cursor/BlendViewCursorOriginActions.h"
#include "Cursor/BlendViewSceneCursorPlacement.h"
#include "Cursor/BlendViewSceneCursorRenderer.h"
#include "Cursor/BlendViewSceneCursorState.h"
#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "EditorViewportClient.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/Actor.h"
#include "ILevelEditor.h"
#include "ISceneOutliner.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "Localization/BlendViewLocalization.h"
#include "Modules/ModuleManager.h"
#include "MouseDeltaTracker.h"
#include "SLevelViewport.h"
#include "Slate/SceneViewport.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Tools/BlendViewTransformResetPolicy.h"
#include "Tools/BlendViewTransformTool.h"
#include "Tools/BlendViewTransformTargetAdapter.h"
#include "Tools/BlendViewTransformSelectionResolver.h"
#include "UI/BlendViewMoveToFolderMenu.h"
#include "UI/BlendViewStatusBarPresenter.h"
#include "Viewport/BlendViewViewportProjector.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlendViewInput, Log, All);

namespace
{
	FBlendViewStatusToken MakeNavigationKeyToken(const TCHAR* Text)
	{
		FBlendViewStatusToken Token;
		Token.Kind = EBlendViewStatusTokenKind::Key;
		Token.Text = FText::FromString(Text);
		return Token;
	}

	FBlendViewStatusToken MakeNavigationMouseToken(const TCHAR* Text)
	{
		FBlendViewStatusToken Token;
		Token.Kind = EBlendViewStatusTokenKind::Mouse;
		Token.Text = FText::FromString(Text);
		return Token;
	}

	TSharedPtr<SWidget> FindLevelViewportWidget(const FEditorViewportClient* ViewportClient)
	{
		if (!ViewportClient || !FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
		{
			return nullptr;
		}

		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		if (const TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor())
		{
			for (const TSharedPtr<SLevelViewport>& Viewport : LevelEditor->GetViewports())
			{
				if (Viewport.IsValid() && &Viewport->GetLevelViewportClient() == ViewportClient)
				{
					return Viewport;
				}
			}
		}
		return nullptr;
	}

	FWidgetPath FindInputRouterWidgetPathUnderCursor()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FWidgetPath();
		}

		FSlateApplication& SlateApplication = FSlateApplication::Get();
		return SlateApplication.LocateWindowUnderMouse(
			SlateApplication.GetCursorPos(),
			SlateApplication.GetInteractiveTopLevelWindows(),
			false);
	}

	bool IsContentBrowserPath(const FWidgetPath& WidgetPath)
	{
		static const FName ContentBrowserType(TEXT("SContentBrowser"));
		static const FName AssetViewType(TEXT("SAssetView"));
		for (int32 Index = WidgetPath.Widgets.Num() - 1; Index >= 0; --Index)
		{
			const FName WidgetType = WidgetPath.Widgets[Index].Widget->GetType();
			if (WidgetType == ContentBrowserType || WidgetType == AssetViewType)
			{
				return true;
			}
		}
		return false;
	}

	bool IsTextEntryFocused()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		const TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
		if (!FocusedWidget.IsValid())
		{
			return false;
		}

		const FString WidgetType = FocusedWidget->GetType().ToString();
		return WidgetType.Contains(TEXT("EditableText")) ||
			WidgetType.Contains(TEXT("SearchBox")) ||
			WidgetType.Contains(TEXT("SuggestionTextBox"));
	}

	bool IsFlightSpeedModifierKey(const FKey& Key)
	{
		return Key == EKeys::LeftShift ||
			Key == EKeys::RightShift
#if BLENDVIEW_UE_5_8_OR_LATER
			|| FBlendViewFlightInputState::IsAltKey(Key)
#endif
			;
	}

	bool IsFlightShiftSpeedModifierKey(const FKey& Key)
	{
		return Key == EKeys::LeftShift ||
			Key == EKeys::RightShift;
	}

}

EBlendViewInputResult FBlendViewInputRouter::RouteInput(const FBlendViewInputEvent& Event)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	FlightInputState.Update(Event);
	if (!(Event.Type == EBlendViewInputEventType::MouseUp && Event.Key == EKeys::RightMouseButton))
	{
		SyncFlightNavigationButtonState();
	}

	if (Event.Type == EBlendViewInputEventType::MouseUp && Event.Key == EKeys::RightMouseButton)
	{
		bSuppressPointerInputUntilRightMouseUp = false;
		if (bSuppressNextRightMouseUp)
		{
			bSuppressNextRightMouseUp = false;
			ReleasePointerState();
			return EBlendViewInputResult::Handled;
		}
	}

	if (bSuppressPointerInputUntilRightMouseUp && IsPointerEvent(Event))
	{
		return EBlendViewInputResult::Handled;
	}

	if (!NavigationController.IsNavigating() && !ToolManager.HasActiveTool() && !GraphTransformController.IsActive())
	{
		if (FlightInputState.IsRightMouseDown() && FlightNavigationContext.IsValid())
		{
			LastViewportContext = FlightNavigationContext;
		}
		else
		{
			const FVector2D PointerScreenPosition = IsPointerEvent(Event)
				? Event.ScreenPosition
				: (FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCursorPos() : FVector2D::ZeroVector);
			LastViewportContext = ViewportResolver.ResolveForMousePosition(PointerScreenPosition);
		}
	}

	CaptureFlightNavigationViewport(Event);

	UpdateNavigationSpeedBoost(Event, Settings);

	if (FBlendViewCommands::IsRegistered() &&
		FBlendViewCommands::Get().ToggleBlendView.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewCommands::Get().ToggleBlendView))
	{
		SetBlendViewEnabled(!bBlendViewEnabled);
		return EBlendViewInputResult::Handled;
	}

	if (!bBlendViewEnabled)
	{
		return EBlendViewInputResult::PassThrough;
	}

	if (TryBeginCursorPlacement(Event))
	{
		return EBlendViewInputResult::Handled;
	}
	if (UpdateCursorPlacement(Event))
	{
		return EBlendViewInputResult::Handled;
	}
	if (const EBlendViewInputResult CursorResult = TryCommitCursorPlacement(Event);
		CursorResult == EBlendViewInputResult::Handled)
	{
		return CursorResult;
	}

	if (GraphTransformController.IsActive())
	{
		if (IsModalCancelInput(Event))
		{
			GraphTransformController.Cancel();
			StateMachine.Reset();
			bSuppressNextRightMouseUp =
				Event.Type == EBlendViewInputEventType::MouseDown &&
				Event.Key == EKeys::RightMouseButton;
			bSuppressPointerInputUntilRightMouseUp = bSuppressNextRightMouseUp;
			ReleasePointerState();
		}
		else if (IsModalConfirmInput(Event))
		{
			GraphTransformController.Confirm();
			StateMachine.Reset();
			ReleasePointerState();
		}
		else
		{
			GraphTransformController.RouteInput(Event);
		}
		return EBlendViewInputResult::Handled;
	}

	if (GraphActionController.TryDeleteAndReconnectUnderCursor(Event))
	{
		return EBlendViewInputResult::Handled;
	}

	if (GraphActionController.TryToggleMaterialPreviewUnderCursor(Event))
	{
		return EBlendViewInputResult::Handled;
	}

	if (ToolManager.HasActiveTool())
	{
		if (ToolManager.WantsInputBeforeModalShortcuts(Event))
		{
			return ToolManager.RouteInput(Event);
		}

		EBlendViewTransformResetChannel ResetChannel = EBlendViewTransformResetChannel::Location;
		if (IsTransformResetInput(Event, ResetChannel))
		{
			TryResetCurrentTransform(ResetChannel);
			return EBlendViewInputResult::Handled;
		}

		if (IsModalCancelInput(Event))
		{
			ToolManager.CancelActiveTool();
			StateMachine.Reset();
			bSuppressNextRightMouseUp =
				Event.Type == EBlendViewInputEventType::MouseDown &&
				Event.Key == EKeys::RightMouseButton;
			bSuppressPointerInputUntilRightMouseUp = bSuppressNextRightMouseUp;
			ReleasePointerState();
			return EBlendViewInputResult::Handled;
		}

		if (IsModalConfirmInput(Event))
		{
			ToolManager.ConfirmActiveTool();
			StateMachine.Reset();
			ReleasePointerState();
			return EBlendViewInputResult::Handled;
		}

		return ToolManager.RouteInput(Event);
	}

	if (NavigationController.IsNavigating())
	{
		const bool bWasAxisViewActive = NavigationController.IsAxisViewActive();
		EBlendViewInputResult NavigationResult = Settings && Settings->bEnableMouseNavigation
			? NavigationController.RouteInput(Event)
			: EBlendViewInputResult::PassThrough;
		if (!Settings || !Settings->bEnableMouseNavigation)
		{
			NavigationController.EndNavigation();
		}
		if (bWasAxisViewActive || NavigationController.IsAxisViewActive())
		{
			bNavigationReleasePassThrough = false;
		}
		if (!NavigationController.IsNavigating())
		{
			if (Event.Type == EBlendViewInputEventType::MouseUp &&
				Event.Key == EKeys::MiddleMouseButton &&
				!bNavigationReleasePassThrough)
			{
				NavigationResult = EBlendViewInputResult::Handled;
			}
			bNavigationReleasePassThrough = true;
			StateMachine.Reset();
			ReleasePointerState();
		}
		UpdateNavigationStatusBar(Settings);
		return NavigationResult;
	}

	if (Settings && Settings->bEnableMouseNavigation && !FlightInputState.IsRightMouseDown() && NavigationController.TryBegin(Event, LastViewportContext))
	{
		bNavigationReleasePassThrough =
			LastViewportContext.Kind == EBlendViewViewportKind::LevelEditor &&
			!NavigationController.IsAxisViewActive();
		StateMachine.EnterNavigation();
		UpdateNavigationStatusBar(Settings);
		return bNavigationReleasePassThrough
			? EBlendViewInputResult::PassThrough
			: EBlendViewInputResult::Handled;
	}

	if (!ShouldBlockNewBlendViewShortcut())
	{
		if (TryFrameSelectedUnderCursor(Event))
		{
			return EBlendViewInputResult::Handled;
		}

		if (TryOpenMoveSelectedToFolderMenu(Event))
		{
			return EBlendViewInputResult::Handled;
		}

		EBlendViewTransformResetChannel ResetChannel = EBlendViewTransformResetChannel::Location;
		if (LastViewportContext.IsValid() && IsTransformResetInput(Event, ResetChannel))
		{
			if (TryResetCurrentTransform(ResetChannel))
			{
				return EBlendViewInputResult::Handled;
			}
		}

		if (!GraphActionController.IsTextInputFocused() && GraphTransformController.TryDuplicateAndBegin(Event))
		{
			StateMachine.EnterModalTool();
			return EBlendViewInputResult::Handled;
		}

		if (!GraphActionController.IsTextInputFocused() && GraphTransformController.TryBegin(Event))
		{
			StateMachine.EnterModalTool();
			return EBlendViewInputResult::Handled;
		}

		const EBlendViewInputResult DuplicateResult = TryBeginDuplicateTranslateTool(Event);
		if (DuplicateResult == EBlendViewInputResult::Handled)
		{
			return DuplicateResult;
		}

		const EBlendViewInputResult TransformResult = TryBeginTransformTool(Event);
		if (TransformResult == EBlendViewInputResult::Handled)
		{
			return TransformResult;
		}
	}

	return ToolManager.HasActiveTool()
		? ToolManager.RouteInput(Event)
		: EBlendViewInputResult::PassThrough;
}

bool FBlendViewInputRouter::RouteFlightNavigationKeyDown(const FKeyEvent& KeyEvent)
{
	return RouteFlightNavigationKey(KeyEvent, true);
}

bool FBlendViewInputRouter::RouteFlightNavigationKeyUp(const FKeyEvent& KeyEvent)
{
	return RouteFlightNavigationKey(KeyEvent, false);
}

bool FBlendViewInputRouter::RouteFlightNavigationKey(
	const FKeyEvent& KeyEvent,
	const bool bPressed)
{
	SyncFlightNavigationButtonState();

	const bool bIsSpeedModifierKey = IsFlightSpeedModifierKey(KeyEvent.GetKey());
	const bool bIsShiftSpeedModifierKey = IsFlightShiftSpeedModifierKey(KeyEvent.GetKey());
#if BLENDVIEW_UE_5_8_OR_LATER
	const bool bIsAltSpeedModifierKey = FBlendViewFlightInputState::IsAltKey(KeyEvent.GetKey());
#else
	constexpr bool bIsAltSpeedModifierKey = false;
#endif

	FBlendViewInputEvent StateEvent;
	StateEvent.Type = bPressed ? EBlendViewInputEventType::KeyDown : EBlendViewInputEventType::KeyUp;
	StateEvent.Key = KeyEvent.GetKey();
	StateEvent.bShiftDown = KeyEvent.IsShiftDown();
	StateEvent.bAltDown = KeyEvent.IsAltDown();
	if (FSlateApplication::IsInitialized())
	{
		const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
		StateEvent.bShiftDown = StateEvent.bShiftDown || ModifierKeys.IsShiftDown();
		StateEvent.bAltDown = StateEvent.bAltDown || ModifierKeys.IsAltDown();
	}
	if (bIsSpeedModifierKey)
	{
		if (bIsShiftSpeedModifierKey)
		{
			StateEvent.bShiftDown = bPressed;
		}
		if (bIsAltSpeedModifierKey)
		{
			StateEvent.bAltDown = bPressed;
		}
	}
	FlightInputState.Update(StateEvent);

	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	const bool bCanHandleNativeFlightBoost =
		bBlendViewEnabled &&
		Settings &&
		Settings->bEnableShiftFlySpeedBoost &&
		FlightInputState.IsRightMouseDown() &&
		(FlightNavigationContext.IsValid() || LastViewportContext.IsValid());

	if (!bCanHandleNativeFlightBoost &&
		!(bIsAltSpeedModifierKey && bSuppressFlightAltUntilKeyUp))
	{
		EndFlightNavigationSession();
		return false;
	}

	if (bIsSpeedModifierKey)
	{
		UpdateFlightNavigationSession();
		if (bPressed && !KeyEvent.IsRepeat())
		{
			RefreshNativeFlightNavigationKeysForSpeedModifier();
		}
		if (bIsAltSpeedModifierKey)
		{
			bSuppressFlightAltUntilKeyUp = FlightInputState.IsAltDown();
		}
		return true;
	}

	return false;
}

void FBlendViewInputRouter::RefreshNativeFlightNavigationKeysForSpeedModifier()
{
#if BLENDVIEW_UE_5_8_OR_LATER
	if (!FlightInputState.IsRightMouseDown() || !FlightInputState.HasNavigationInput())
	{
		return;
	}

	if (!FlightNavigationViewport)
	{
		FlightNavigationViewport = FlightNavigationContext.SceneViewport;
	}
	if (!FlightNavigationViewport)
	{
		return;
	}

	TArray<FKey> PressedNavigationKeys;
	FlightInputState.AppendPressedNavigationKeys(PressedNavigationKeys);
	for (const FKey& Key : PressedNavigationKeys)
	{
		if (FlightNavigationViewport->KeyState(Key))
		{
			continue;
		}

		const FKeyEvent KeyEvent(
			Key,
			FModifierKeysState(),
			0,
			false,
			0,
			0);
		FlightNavigationViewport->OnKeyDown(FGeometry(), KeyEvent);
	}
#endif
}

void FBlendViewInputRouter::Tick(float DeltaTime)
{
	if (ToolManager.Tick(DeltaTime))
	{
		StateMachine.Reset();
		ReleasePointerState();
	}
	SyncFlightNavigationButtonState();
	UpdateFlightNavigationSession();
}

void FBlendViewInputRouter::DrawHUD(UCanvas* Canvas)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (bBlendViewEnabled && Settings && Settings->bEnableSceneCursor)
	{
		FBlendViewSceneCursorRenderer::Draw(Canvas, SceneCursor);
	}
	if (bBlendViewEnabled)
	{
		ToolManager.DrawHUD(Canvas);
	}
}

bool FBlendViewInputRouter::TryBeginCursorPlacement(const FBlendViewInputEvent& Event)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings ||
		!Settings->bEnableSceneCursor ||
		Event.Type != EBlendViewInputEventType::MouseDown ||
		Event.Key != EKeys::RightMouseButton ||
		!Event.bShiftDown ||
		Event.bControlDown ||
		Event.bAltDown ||
		Event.bCommandDown ||
		ToolManager.HasActiveTool() ||
		GraphTransformController.IsActive() ||
		NavigationController.IsNavigating())
	{
		return false;
	}

	FBlendViewViewportContext Context = ViewportResolver.ResolveForMousePosition(Event.ScreenPosition);
	if (!Context.IsValid() || Context.Kind != EBlendViewViewportKind::LevelEditor)
	{
		return false;
	}

	bPendingCursorPlacement = true;
	CursorPlacementStartScreenPosition = Event.ScreenPosition;
	CursorPlacementContext = Context;
	return true;
}

bool FBlendViewInputRouter::UpdateCursorPlacement(const FBlendViewInputEvent& Event)
{
	if (!bPendingCursorPlacement)
	{
		return false;
	}

	if (Event.Type == EBlendViewInputEventType::MouseMove)
	{
		constexpr double CursorClickMoveThresholdSquared = 36.0;
		if (FVector2D::DistSquared(Event.ScreenPosition, CursorPlacementStartScreenPosition) >
			CursorClickMoveThresholdSquared)
		{
			bSuppressNextRightMouseUp = true;
			bSuppressPointerInputUntilRightMouseUp = true;
			CancelCursorPlacement();
		}
		return true;
	}

	if (Event.Type == EBlendViewInputEventType::MouseDown &&
		Event.Key != EKeys::RightMouseButton)
	{
		bSuppressNextRightMouseUp = true;
		bSuppressPointerInputUntilRightMouseUp = true;
		CancelCursorPlacement();
		return true;
	}

	if (Event.Type == EBlendViewInputEventType::KeyDown &&
		FBlendViewFlightInputState::IsNavigationKey(Event.Key))
	{
		bSuppressNextRightMouseUp = true;
		bSuppressPointerInputUntilRightMouseUp = true;
		CancelCursorPlacement();
		return true;
	}

	return false;
}

EBlendViewInputResult FBlendViewInputRouter::TryCommitCursorPlacement(const FBlendViewInputEvent& Event)
{
	if (!bPendingCursorPlacement ||
		Event.Type != EBlendViewInputEventType::MouseUp ||
		Event.Key != EKeys::RightMouseButton)
	{
		return EBlendViewInputResult::PassThrough;
	}

	const bool bCanPlaceCursor =
		!FlightInputState.HasNavigationInput() &&
		CursorPlacementContext.IsValid() &&
		CursorPlacementContext.Kind == EBlendViewViewportKind::LevelEditor;
	const FBlendViewViewportContext Context = CursorPlacementContext;
	CancelCursorPlacement();

	if (!bCanPlaceCursor)
	{
		ReleasePointerState();
		RestorePointerAfterCursorPlacement(Context);
		return EBlendViewInputResult::Handled;
	}

	SetCursorFromViewportClick(Context, Context.MouseViewportPosition);
	LastViewportContext = Context;
	ReleasePointerState();
	RestorePointerAfterCursorPlacement(Context);
	return EBlendViewInputResult::Handled;
}

void FBlendViewInputRouter::CancelCursorPlacement()
{
	bPendingCursorPlacement = false;
	CursorPlacementStartScreenPosition = FVector2D::ZeroVector;
	CursorPlacementContext = FBlendViewViewportContext();
}

bool FBlendViewInputRouter::SetCursorFromViewportClick(
	const FBlendViewViewportContext& Context,
	const FVector2D& ViewportPosition)
{
	FBlendViewSceneCursorPlacementResult Placement;
	const FBlendViewSceneCursorPlacementOptions PlacementOptions;
	if (!FBlendViewSceneCursorPlacement::Resolve(
			Context,
			ViewportPosition,
			SceneCursor,
			PlacementOptions,
			Placement))
	{
		return false;
	}

	FBlendViewSceneCursorState::SetTransform(SceneCursor, Placement.Transform);
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
	return true;
}

void FBlendViewInputRouter::RestorePointerAfterCursorPlacement(const FBlendViewViewportContext& Context)
{
	if (Context.ViewportClient)
	{
		Context.ViewportClient->SetRequiredCursorOverride(false);
	}
	if (Context.Viewport)
	{
		Context.Viewport->CaptureMouse(false);
		Context.Viewport->LockMouseToViewport(false);
	}

	if (FSlateApplication::IsInitialized())
	{
		if (const TSharedPtr<FSlateUser> CursorUser = FSlateApplication::Get().GetCursorUser())
		{
			CursorUser->SetCursorVisibility(true);
		}
		FSlateApplication::Get().QueryCursor();
	}
}

void FBlendViewInputRouter::CancelActiveOperation()
{
	NavigationController.EndNavigation();
	GraphTransformController.Cancel();
	ToolManager.CancelActiveTool();
	StateMachine.Reset();
	FlightInputState.Reset();
	bSuppressFlightAltUntilKeyUp = false;
	bSuppressNextRightMouseUp = false;
	bSuppressPointerInputUntilRightMouseUp = false;
	bNavigationReleasePassThrough = true;
	CancelCursorPlacement();
	FBlendViewStatusBarPresenter::Get().Restore();
	ReleasePointerState();
}

void FBlendViewInputRouter::PrepareForEngineExit()
{
	NavigationController.EndNavigation();
	GraphTransformController.PrepareForEngineExit();
	ToolManager.PrepareForEngineExit();
	StateMachine.Reset();
	EndFlightNavigationSession();
	FlightNavigationContext = FBlendViewViewportContext();
	FlightNavigationLifetimeGuard.Reset();
	LastViewportContext = FBlendViewViewportContext();
	FlightInputState.Reset();
	bSuppressFlightAltUntilKeyUp = false;
	bSuppressNextRightMouseUp = false;
	bSuppressPointerInputUntilRightMouseUp = false;
	bNavigationReleasePassThrough = true;
	CancelCursorPlacement();
}

bool FBlendViewInputRouter::IsPointerEvent(const FBlendViewInputEvent& Event) const
{
	return Event.Type == EBlendViewInputEventType::MouseMove ||
		Event.Type == EBlendViewInputEventType::MouseDown ||
		Event.Type == EBlendViewInputEventType::MouseUp ||
		Event.Type == EBlendViewInputEventType::MouseDoubleClick ||
		Event.Type == EBlendViewInputEventType::MouseWheel;
}

bool FBlendViewInputRouter::IsModalCancelInput(const FBlendViewInputEvent& Event) const
{
	return (Event.Type == EBlendViewInputEventType::KeyDown && Event.Key == EKeys::Escape) ||
		(Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::RightMouseButton);
}

bool FBlendViewInputRouter::IsModalConfirmInput(const FBlendViewInputEvent& Event) const
{
	return (Event.Type == EBlendViewInputEventType::KeyDown && Event.Key == EKeys::Enter) ||
		(Event.Type == EBlendViewInputEventType::KeyDown && Event.Key == EKeys::SpaceBar) ||
		(Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::LeftMouseButton);
}

bool FBlendViewInputRouter::IsTransformResetInput(
	const FBlendViewInputEvent& Event,
	EBlendViewTransformResetChannel& OutChannel) const
{
	if (Event.Type != EBlendViewInputEventType::KeyDown ||
		Event.bIsRepeat ||
		!Event.bAltDown ||
		Event.bControlDown ||
		Event.bShiftDown ||
		Event.bCommandDown)
	{
		return false;
	}

	if (Event.Key == EKeys::G)
	{
		OutChannel = EBlendViewTransformResetChannel::Location;
		return true;
	}
	if (Event.Key == EKeys::R)
	{
		OutChannel = EBlendViewTransformResetChannel::Rotation;
		return true;
	}
	if (Event.Key == EKeys::S)
	{
		OutChannel = EBlendViewTransformResetChannel::Scale;
		return true;
	}

	return false;
}

bool FBlendViewInputRouter::ResetSelectedActorTransform(const EBlendViewTransformMode Mode)
{
	if (!GEditor)
	{
		return false;
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors)
	{
		return false;
	}

	TArray<AActor*> Actors;
	SelectedActors->GetSelectedObjects<AActor>(Actors);
	Actors.RemoveAll([Mode](const AActor* Actor)
	{
		if (!IsValid(Actor) || Actor->IsTemplate())
		{
			return true;
		}

		switch (Mode)
		{
		case EBlendViewTransformMode::Translate:
			return Actor->GetActorLocation().IsNearlyZero();
		case EBlendViewTransformMode::Rotate:
			return Actor->GetActorRotation().IsNearlyZero();
		case EBlendViewTransformMode::Scale:
			return Actor->GetActorScale3D().Equals(FVector::OneVector);
		default:
			return true;
		}
	});
	if (Actors.IsEmpty())
	{
		return false;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("BlendView", "ResetActorTransform", "BlendView Reset Actor Transform"));
	for (AActor* Actor : Actors)
	{
		Actor->Modify();
		switch (Mode)
		{
		case EBlendViewTransformMode::Translate:
			Actor->SetActorLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
			break;
		case EBlendViewTransformMode::Rotate:
			Actor->SetActorRotation(FRotator::ZeroRotator, ETeleportType::TeleportPhysics);
			break;
		case EBlendViewTransformMode::Scale:
			Actor->SetActorScale3D(FVector::OneVector);
			break;
		default:
			break;
		}
	}

	GEditor->NoteSelectionChange();
	GEditor->RedrawLevelEditingViewports(true);

	return true;
}

bool FBlendViewInputRouter::TryResetCurrentTransform(const EBlendViewTransformResetChannel Channel)
{
	if (ToolManager.HasActiveTool())
	{
		return ToolManager.TryResetCurrentTransform(Channel);
	}

	if (LastViewportContext.Kind == EBlendViewViewportKind::LevelEditor &&
		FBlendViewTransformTargetAdapter::IsActiveModelingPivotToolAvailable())
	{
		switch (Channel)
		{
		case EBlendViewTransformResetChannel::Location:
			return FBlendViewTransformTargetAdapter::ResetActiveModelingPivotLocation();
		case EBlendViewTransformResetChannel::Rotation:
			return FBlendViewTransformTargetAdapter::ResetActiveModelingPivotRotation();
		case EBlendViewTransformResetChannel::Scale:
			return true;
		default:
			return true;
		}
	}

	const TOptional<EBlendViewTransformMode> FallbackMode =
		FBlendViewTransformResetPolicy::GetFallbackMode(Channel);
	return FallbackMode.IsSet() && ResetSelectedTransform(FallbackMode.GetValue());
}

bool FBlendViewInputRouter::ResetSelectedTransform(const EBlendViewTransformMode Mode)
{
	if (LastViewportContext.Kind == EBlendViewViewportKind::LevelEditor)
	{
		return ResetSelectedActorTransform(Mode);
	}
	if (LastViewportContext.Kind == EBlendViewViewportKind::EditorViewport)
	{
		return ResetSelectedNativeComponentTransform(Mode);
	}
	return false;
}

bool FBlendViewInputRouter::ResetSelectedNativeComponentTransform(
	const EBlendViewTransformMode Mode)
{
	if (!GEditor || !LastViewportContext.ViewportClient)
	{
		return false;
	}

	UWorld* ViewportWorld = LastViewportContext.ViewportClient->GetWorld();
	if (!ViewportWorld)
	{
		return false;
	}

	TArray<USceneComponent*> Components;
	FBlendViewTransformSelectionResolver::ResolveNativeSceneComponents(
		LastViewportContext,
		Components);

	Components.RemoveAll([Mode](const USceneComponent* Component)
	{
		if (!IsValid(Component) ||
			(!Component->IsA<UPrimitiveComponent>() &&
				!Component->IsA<UChildActorComponent>()))
		{
			return true;
		}

		switch (Mode)
		{
		case EBlendViewTransformMode::Translate:
			return Component->GetRelativeLocation().IsNearlyZero();
		case EBlendViewTransformMode::Rotate:
			return Component->GetRelativeRotation().IsNearlyZero();
		case EBlendViewTransformMode::Scale:
			return Component->GetRelativeScale3D().Equals(FVector::OneVector);
		default:
			return true;
		}
	});
	if (Components.IsEmpty())
	{
		return false;
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("BlendView", "ResetComponentTransform", "BlendView Reset Component Transform"));
	for (USceneComponent* Component : Components)
	{
		Component->Modify();
		switch (Mode)
		{
		case EBlendViewTransformMode::Translate:
			Component->SetRelativeLocation(FVector::ZeroVector);
			break;
		case EBlendViewTransformMode::Rotate:
			Component->SetRelativeRotation(FRotator::ZeroRotator);
			break;
		case EBlendViewTransformMode::Scale:
			Component->SetRelativeScale3D(FVector::OneVector);
			break;
		default:
			break;
		}
		Component->PostEditComponentMove(false);
		Component->MarkPackageDirty();

		USceneComponent* TemplateComponent = Cast<USceneComponent>(Component->GetArchetype());
		if (!TemplateComponent ||
			TemplateComponent == Component ||
			TemplateComponent->GetWorld() == ViewportWorld)
		{
			continue;
		}

		TemplateComponent->Modify();
		switch (Mode)
		{
		case EBlendViewTransformMode::Translate:
			TemplateComponent->SetRelativeLocation(FVector::ZeroVector);
			break;
		case EBlendViewTransformMode::Rotate:
			TemplateComponent->SetRelativeRotation(FRotator::ZeroRotator);
			break;
		case EBlendViewTransformMode::Scale:
			TemplateComponent->SetRelativeScale3D(FVector::OneVector);
			break;
		default:
			break;
		}
		TemplateComponent->PostEditComponentMove(false);
		TemplateComponent->MarkPackageDirty();
	}

	LastViewportContext.ViewportClient->Invalidate();
	FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
	return true;
}

bool FBlendViewInputRouter::TryFrameSelectedUnderCursor(const FBlendViewInputEvent& Event)
{
	if (!FBlendViewCommands::IsRegistered() ||
		!FBlendViewCommands::Get().FrameSelected.IsValid() ||
		!FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewCommands::Get().FrameSelected))
	{
		return false;
	}

	if (GraphActionController.TryFrameGraphUnderCursor(Event))
	{
		return true;
	}

	if (LastViewportContext.Kind == EBlendViewViewportKind::LevelEditor)
	{
		return FrameSelectedInViewport();
	}

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		return false;
	}

	const FWidgetPath WidgetPath = FindInputRouterWidgetPathUnderCursor();
	if (!WidgetPath.IsValid())
	{
		return false;
	}

	FLevelEditorModule& LevelEditorModule =
		FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	const TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (LevelEditor.IsValid())
	{
		for (const TWeakPtr<ISceneOutliner>& WeakOutliner : LevelEditor->GetAllSceneOutliners())
		{
			if (const TSharedPtr<ISceneOutliner> Outliner = WeakOutliner.Pin();
				Outliner.IsValid() && WidgetPath.ContainsWidget(Outliner.Get()))
			{
				Outliner->FrameSelectedItems();
				return true;
			}
		}
	}

	if (IsContentBrowserPath(WidgetPath) &&
		FLevelEditorActionCallbacks::FindInContentBrowser_CanExecute())
	{
		FLevelEditorActionCallbacks::FindInContentBrowser_Clicked();
		return true;
	}

	return false;
}

bool FBlendViewInputRouter::FrameSelectedInViewport() const
{
	if (!GEditor || !LastViewportContext.ViewportClient)
	{
		return false;
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors)
	{
		return false;
	}

	TArray<AActor*> Actors;
	SelectedActors->GetSelectedObjects<AActor>(Actors);
	FBox SelectionBounds(ForceInit);
	for (const AActor* Actor : Actors)
	{
		if (IsValid(Actor) && !Actor->IsTemplate())
		{
			const FBox ActorBounds = Actor->GetComponentsBoundingBox(true);
			if (ActorBounds.IsValid)
			{
				SelectionBounds += ActorBounds;
			}
			else
			{
				SelectionBounds += Actor->GetActorLocation();
			}
		}
	}

	if (!SelectionBounds.IsValid)
	{
		return false;
	}

	LastViewportContext.ViewportClient->FocusViewportOnBox(SelectionBounds);
	return true;
}

bool FBlendViewInputRouter::TryOpenMoveSelectedToFolderMenu(const FBlendViewInputEvent& Event)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings ||
		!Settings->bEnableMoveToFolderMenu ||
		!FBlendViewCommands::IsRegistered() ||
		!FBlendViewCommands::Get().MoveSelectedToFolder.IsValid() ||
		!FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewCommands::Get().MoveSelectedToFolder) ||
		LastViewportContext.Kind != EBlendViewViewportKind::LevelEditor ||
		!LastViewportContext.IsValid())
	{
		return false;
	}

	const FVector2D MenuPosition = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetCursorPos()
		: Event.ScreenPosition;
	return FBlendViewMoveToFolderMenu::Open(LastViewportContext, MenuPosition);
}

EBlendViewInputResult FBlendViewInputRouter::TryBeginDuplicateTranslateTool(const FBlendViewInputEvent& Event)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings || !Settings->bEnableTransformWorkflow)
	{
		return EBlendViewInputResult::PassThrough;
	}

	if (IsFlightNavigationInputActive() ||
		!GEditor ||
		!FBlendViewCommands::IsRegistered() ||
		!FBlendViewCommands::Get().DuplicateAndTranslate.IsValid() ||
		!FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewCommands::Get().DuplicateAndTranslate) ||
		!LastViewportContext.IsValid())
	{
		return EBlendViewInputResult::PassThrough;
	}

	if (LastViewportContext.Kind == EBlendViewViewportKind::EditorViewport)
	{
		TArray<TWeakObjectPtr<USceneComponent>> DuplicatedComponents;
		TUniquePtr<FScopedTransaction> DuplicateTransaction;
		if (!FBlendViewTransformSelectionResolver::DuplicateBlueprintSubobjects(
			LastViewportContext,
			DuplicatedComponents,
			DuplicateTransaction))
		{
			return EBlendViewInputResult::PassThrough;
		}
		DuplicateTransaction.Reset();

		if (ToolManager.BeginTool(
			MakeUnique<FBlendViewTransformTool>(
				EBlendViewTransformMode::Translate,
				nullptr,
				// Capture the duplicated components' actual pivot instead of reusing
				// the pre-duplicate viewport widget location.
				TOptional<FVector>(),
				MoveTemp(DuplicatedComponents)),
			LastViewportContext))
		{
			StateMachine.EnterModalTool();
		}
		return EBlendViewInputResult::Handled;
	}

	if (LastViewportContext.Kind != EBlendViewViewportKind::LevelEditor)
	{
		return EBlendViewInputResult::PassThrough;
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors)
	{
		return EBlendViewInputResult::PassThrough;
	}

	TArray<AActor*> ActorsToDuplicate;
	SelectedActors->GetSelectedObjects<AActor>(ActorsToDuplicate);
	ActorsToDuplicate.RemoveAll([](const AActor* Actor)
	{
		return !IsValid(Actor) || Actor->IsTemplate();
	});
	if (ActorsToDuplicate.IsEmpty())
	{
		return EBlendViewInputResult::PassThrough;
	}

	UEditorActorSubsystem* ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!ActorSubsystem)
	{
		return EBlendViewInputResult::PassThrough;
	}

	UWorld* TargetWorld = ActorsToDuplicate[0]->GetWorld();
	const FVector DuplicatePivot = LastViewportContext.ViewportClient->GetWidgetLocation();
	TUniquePtr<FScopedTransaction> DuplicateTransaction = MakeUnique<FScopedTransaction>(
		NSLOCTEXT("BlendView", "DuplicateActors", "BlendView Duplicate Actors"));
	UEditorActorSubsystem::FActorDuplicateParameters DuplicateParameters;
	DuplicateParameters.bTransact = false;
	TArray<AActor*> DuplicatedActors = ActorSubsystem->DuplicateActors(
		ActorsToDuplicate,
		TargetWorld,
		FVector::ZeroVector,
		DuplicateParameters);
	DuplicatedActors.RemoveAll([](const AActor* Actor)
	{
		return !IsValid(Actor) || Actor->IsTemplate();
	});
	if (DuplicatedActors.IsEmpty())
	{
		DuplicateTransaction->Cancel();
		return EBlendViewInputResult::PassThrough;
	}

	ActorSubsystem->SetSelectedLevelActors(DuplicatedActors);
	GEditor->NoteSelectionChange();
	GEditor->RedrawLevelEditingViewports(false);
	DuplicateTransaction.Reset();

	if (ToolManager.BeginTool(
		MakeUnique<FBlendViewTransformTool>(
			EBlendViewTransformMode::Translate,
			nullptr,
			DuplicatePivot),
		LastViewportContext))
	{
		StateMachine.EnterModalTool();
		return EBlendViewInputResult::Handled;
	}

	for (AActor* Actor : DuplicatedActors)
	{
		if (UWorld* World = Actor->GetWorld())
		{
			World->EditorDestroyActor(Actor, true);
		}
	}
	return EBlendViewInputResult::Handled;
}

EBlendViewInputResult FBlendViewInputRouter::TryBeginTransformTool(const FBlendViewInputEvent& Event)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings || !Settings->bEnableTransformWorkflow)
	{
		return EBlendViewInputResult::PassThrough;
	}

	if (IsFlightNavigationInputActive() ||
		!FBlendViewCommands::IsRegistered() ||
		!LastViewportContext.IsValid())
	{
		if (Event.Type == EBlendViewInputEventType::KeyDown)
		{
			UE_LOG(LogBlendViewInput, Verbose, TEXT("BlendView transform rejected before viewport check: Key=%s FlightInput=%d Commands=%d ContextValid=%d"),
				*Event.Key.ToString(),
				IsFlightNavigationInputActive() ? 1 : 0,
				FBlendViewCommands::IsRegistered() ? 1 : 0,
				LastViewportContext.IsValid() ? 1 : 0);
		}
		return EBlendViewInputResult::PassThrough;
	}

	const bool bCanTransformInViewport =
		LastViewportContext.Kind == EBlendViewViewportKind::LevelEditor ||
		(LastViewportContext.Kind == EBlendViewViewportKind::EditorViewport &&
			LastViewportContext.ViewportClient);
	if (!bCanTransformInViewport)
	{
		if (Event.Type == EBlendViewInputEventType::KeyDown)
		{
			const FEditorViewportClient* ViewportClient = LastViewportContext.ViewportClient;
			const FString ViewportClientName = ViewportClient
				? ViewportClient->GetReferencerName()
				: FString(TEXT("None"));
			UE_LOG(LogBlendViewInput, Verbose, TEXT("BlendView transform rejected: Key=%s Kind=%d Client=%s WidgetMode=%d ShowWidget=%d"),
				*Event.Key.ToString(),
				static_cast<int32>(LastViewportContext.Kind),
				*ViewportClientName,
				ViewportClient ? static_cast<int32>(ViewportClient->GetWidgetMode()) : -1,
				ViewportClient && ViewportClient->GetShowWidget() ? 1 : 0);
		}
		return EBlendViewInputResult::PassThrough;
	}

	TOptional<EBlendViewTransformMode> RequestedMode;
	const FBlendViewCommands& Commands = FBlendViewCommands::Get();
	if (Commands.BeginTranslate.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(Event, *Commands.BeginTranslate))
	{
		RequestedMode = EBlendViewTransformMode::Translate;
	}
	else if (Commands.BeginRotate.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(Event, *Commands.BeginRotate))
	{
		RequestedMode = EBlendViewTransformMode::Rotate;
	}
	else if (Commands.BeginScale.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(Event, *Commands.BeginScale))
	{
		RequestedMode = EBlendViewTransformMode::Scale;
	}
	else if (Commands.BeginMirror.IsValid() &&
		FBlendViewCommands::MatchesInputEvent(Event, *Commands.BeginMirror))
	{
		RequestedMode = EBlendViewTransformMode::Mirror;
	}

	if (!RequestedMode.IsSet())
	{
		return EBlendViewInputResult::PassThrough;
	}

	if (ToolManager.BeginTool(MakeUnique<FBlendViewTransformTool>(RequestedMode.GetValue()), LastViewportContext))
	{
		StateMachine.EnterModalTool();
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::KeyDown)
	{
		const FEditorViewportClient* ViewportClient = LastViewportContext.ViewportClient;
		const FString ViewportClientName = ViewportClient
			? ViewportClient->GetReferencerName()
			: FString(TEXT("None"));
		UE_LOG(LogBlendViewInput, Verbose, TEXT("BlendView transform begin failed: Key=%s Kind=%d Client=%s WidgetMode=%d ShowWidget=%d"),
			*Event.Key.ToString(),
			static_cast<int32>(LastViewportContext.Kind),
			*ViewportClientName,
			ViewportClient ? static_cast<int32>(ViewportClient->GetWidgetMode()) : -1,
			ViewportClient && ViewportClient->GetShowWidget() ? 1 : 0);
	}

	return EBlendViewInputResult::PassThrough;
}

bool FBlendViewInputRouter::ShouldBlockNewBlendViewShortcut() const
{
	return IsTextEntryFocused() ||
		IsFlightNavigationInputActive() ||
		IsMouseNavigationButtonDownInViewport() ||
		NavigationController.IsNavigating();
}

bool FBlendViewInputRouter::IsViewportMouseButtonDown(const FKey& Key) const
{
	auto IsButtonDown = [&Key](const FBlendViewViewportContext& Context)
	{
		return Context.SceneViewport && Context.SceneViewport->KeyState(Key);
	};

	if (IsButtonDown(FlightNavigationContext) || IsButtonDown(LastViewportContext))
	{
		return true;
	}

	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FBlendViewViewportContext CursorContext =
		ViewportResolver.ResolveForMousePosition(FSlateApplication::Get().GetCursorPos());
	return IsButtonDown(CursorContext);
}

bool FBlendViewInputRouter::IsFlightNavigationButtonDownInViewport() const
{
	return IsViewportMouseButtonDown(EKeys::RightMouseButton);
}

bool FBlendViewInputRouter::IsMouseNavigationButtonDownInViewport() const
{
	return IsViewportMouseButtonDown(EKeys::MiddleMouseButton);
}

void FBlendViewInputRouter::SyncFlightNavigationButtonState()
{
	if (FlightInputState.IsRightMouseDown())
	{
		if (!IsFlightNavigationButtonDownInViewport())
		{
			FlightInputState.SetRightMouseDown(false);
			EndFlightNavigationSession();
			FlightNavigationContext = FBlendViewViewportContext();
			FlightNavigationLifetimeGuard.Reset();
		}
		else if (!FlightNavigationContext.IsValid())
		{
			if (LastViewportContext.IsValid())
			{
				FlightNavigationContext = LastViewportContext;
			}
			else if (FSlateApplication::IsInitialized())
			{
				FlightNavigationContext = ViewportResolver.ResolveForMousePosition(
					FSlateApplication::Get().GetCursorPos());
			}
			FlightNavigationLifetimeGuard = FlightNavigationContext.ViewportWidget.Pin();
		}
		return;
	}

	if (!IsFlightNavigationButtonDownInViewport())
	{
		return;
	}

	if (!FlightNavigationContext.IsValid())
	{
		if (LastViewportContext.IsValid())
		{
			FlightNavigationContext = LastViewportContext;
		}
		else if (FSlateApplication::IsInitialized())
		{
			FlightNavigationContext = ViewportResolver.ResolveForMousePosition(
				FSlateApplication::Get().GetCursorPos());
		}
	}
	if (FlightNavigationContext.IsValid())
	{
		FlightNavigationLifetimeGuard = FlightNavigationContext.ViewportWidget.Pin();
		FlightInputState.SetRightMouseDown(true);
	}
}

bool FBlendViewInputRouter::IsFlightNavigationInputActive() const
{
	return FlightInputState.IsRightMouseDown() || IsFlightNavigationButtonDownInViewport();
}

void FBlendViewInputRouter::CaptureFlightNavigationViewport(
	const FBlendViewInputEvent& Event)
{
	if (Event.Type == EBlendViewInputEventType::MouseDown &&
		Event.Key == EKeys::RightMouseButton &&
		LastViewportContext.IsValid())
	{
		FlightNavigationContext = LastViewportContext;
		FlightNavigationLifetimeGuard = FlightNavigationContext.ViewportWidget.Pin();
		if (Event.bAltDown)
		{
			SuppressViewportAltStateForFlight();
		}
	}
	else if (Event.Type == EBlendViewInputEventType::MouseUp &&
		Event.Key == EKeys::RightMouseButton)
	{
		EndFlightNavigationSession();
		FlightNavigationContext = FBlendViewViewportContext();
		FlightNavigationLifetimeGuard.Reset();
	}
}

void FBlendViewInputRouter::SuppressViewportAltStateForFlight()
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!bBlendViewEnabled ||
		!Settings ||
		!Settings->bEnableShiftFlySpeedBoost ||
		!FlightInputState.IsRightMouseDown() ||
		!FlightNavigationContext.IsValid())
	{
		return;
	}

	FlightNavigationViewport = FlightNavigationContext.SceneViewport;
	if (!FlightNavigationViewport)
	{
		return;
	}

	bool bSuppressedViewportAlt = false;
	auto ReleaseViewportAltKey = [this, &bSuppressedViewportAlt](const FKey& Key)
	{
		if (FlightNavigationViewport->KeyState(Key))
		{
			FlightNavigationViewport->OnKeyUp(
				FGeometry(),
				FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
			bSuppressedViewportAlt = true;
		}
	};
	ReleaseViewportAltKey(EKeys::LeftAlt);
	ReleaseViewportAltKey(EKeys::RightAlt);

	if (bSuppressedViewportAlt ||
		(FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsAltDown()))
	{
		bSuppressFlightAltUntilKeyUp = true;
	}
}

bool FBlendViewInputRouter::EnsureFlightNavigationSession(const float SpeedMultiplier)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!bBlendViewEnabled ||
		!Settings ||
		!Settings->bEnableShiftFlySpeedBoost ||
		!FlightInputState.IsRightMouseDown() ||
		FMath::IsNearlyEqual(SpeedMultiplier, 1.0f))
	{
		return false;
	}

	if (!FlightNavigationContext.IsValid())
	{
		if (LastViewportContext.IsValid())
		{
			FlightNavigationContext = LastViewportContext;
		}
		else if (FSlateApplication::IsInitialized())
		{
			FlightNavigationContext = ViewportResolver.ResolveForMousePosition(
				FSlateApplication::Get().GetCursorPos());
		}
	}
	if (!FlightNavigationContext.IsValid())
	{
		return false;
	}
	FlightNavigationLifetimeGuard = FlightNavigationContext.ViewportWidget.Pin();
	if (!FlightNavigationLifetimeGuard.IsValid())
	{
		FlightNavigationContext = FBlendViewViewportContext();
		return false;
	}

	FlightSpeedBoostSession.Activate(
		FlightNavigationContext.ViewportClient,
		SpeedMultiplier,
		FlightInputState.IsShiftDown() && !FlightInputState.IsAltDown());
	FlightNavigationViewport = FlightNavigationContext.SceneViewport;
	return FlightNavigationViewport != nullptr;
}

void FBlendViewInputRouter::EndFlightNavigationSession()
{
	FlightNavigationViewport = nullptr;
	FlightSpeedBoostSession.Deactivate();
}

void FBlendViewInputRouter::UpdateNavigationSpeedBoost(
	const FBlendViewInputEvent& Event,
	const UBlendViewSettings* Settings)
{
	(void)Event;
	(void)Settings;
	UpdateFlightNavigationSession();
}

void FBlendViewInputRouter::SyncFlightNavigationState()
{
	if (FSlateApplication::IsInitialized())
	{
		const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
		FlightInputState.SetModifierState(
			ModifierKeys.IsShiftDown(),
			ModifierKeys.IsAltDown());
	}
}

void FBlendViewInputRouter::UpdateFlightNavigationSession()
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!bBlendViewEnabled ||
		!Settings ||
		!Settings->bEnableShiftFlySpeedBoost ||
		!FlightInputState.IsRightMouseDown())
	{
		EndFlightNavigationSession();
		return;
	}

	SyncFlightNavigationState();

	if (!FlightInputState.IsShiftDown())
	{
		EndFlightNavigationSession();
		return;
	}

	const float SpeedMultiplier = FMath::Max(Settings->ShiftFlySpeedMultiplier, 1.0f);
	if (!EnsureFlightNavigationSession(SpeedMultiplier) ||
		!FlightNavigationContext.ViewportClient)
	{
		return;
	}
}

void FBlendViewInputRouter::UpdateNavigationStatusBar(const UBlendViewSettings* Settings)
{
	if (!NavigationController.IsNavigating() || !FBlendViewSessionState::AreStatusHintsVisible())
	{
		FBlendViewStatusBarPresenter::Get().Restore();
		return;
	}

	FBlendViewStatusLine Line;
	auto AddHint = [&Line](const TCHAR* Chinese, const TCHAR* English) -> FBlendViewStatusHint&
	{
		FBlendViewStatusHint& Hint = Line.Hints.AddDefaulted_GetRef();
		Hint.Label = FBlendViewLocalization::Text(Chinese, English);
		return Hint;
	};
	auto AddCancelHint = [&AddHint]() -> FBlendViewStatusHint&
	{
		return AddHint(TEXT("取消"), TEXT("Cancel"));
	};
	auto AddAxisSnapHint = [&AddHint, this]() -> FBlendViewStatusHint&
	{
		switch (NavigationController.GetAxisView())
		{
		case EBlendViewAxisView::Front:
			return AddHint(TEXT("前视图"), TEXT("Front View"));
		case EBlendViewAxisView::Back:
			return AddHint(TEXT("后视图"), TEXT("Back View"));
		case EBlendViewAxisView::Left:
			return AddHint(TEXT("左视图"), TEXT("Left View"));
		case EBlendViewAxisView::Right:
			return AddHint(TEXT("右视图"), TEXT("Right View"));
		case EBlendViewAxisView::Top:
			return AddHint(TEXT("顶视图"), TEXT("Top View"));
		case EBlendViewAxisView::Bottom:
			return AddHint(TEXT("底视图"), TEXT("Bottom View"));
		default:
			return AddHint(TEXT("轴向吸附"), TEXT("Axis Snap"));
		}
	};

	switch (NavigationController.GetMode())
	{
	case EBlendViewNavigationMode::Orbit:
		{
			AddCancelHint().Tokens.Add(MakeNavigationMouseToken(TEXT("RMB")));
			FBlendViewStatusHint& AxisHint = AddAxisSnapHint();
			AxisHint.Tokens.Add(MakeNavigationKeyToken(TEXT("Alt")));
		}
		break;
	case EBlendViewNavigationMode::Pan:
		{
			AddCancelHint().Tokens.Add(MakeNavigationMouseToken(TEXT("RMB")));
		}
		break;
	case EBlendViewNavigationMode::Dolly:
		{
			AddCancelHint().Tokens.Add(MakeNavigationMouseToken(TEXT("RMB")));
		}
		break;
	default:
		break;
	}

	FBlendViewStatusBarPresenter::Get().Present(
		Line,
		LastViewportContext.ViewportWidget.IsValid()
			? LastViewportContext.ViewportWidget.Pin()
			: FindLevelViewportWidget(LastViewportContext.ViewportClient));
}

void FBlendViewInputRouter::ReleasePointerState()
{
	EndFlightNavigationSession();
	FlightNavigationContext = FBlendViewViewportContext();
	FlightNavigationLifetimeGuard.Reset();

	if (LastViewportContext.ViewportClient && LastViewportContext.ViewportWidget.IsValid())
	{
		if (FMouseDeltaTracker* MouseDeltaTracker = LastViewportContext.ViewportClient->GetMouseDeltaTracker())
		{
			MouseDeltaTracker->EndTracking(LastViewportContext.ViewportClient);
			MouseDeltaTracker->ResetUsedDragModifier();
			MouseDeltaTracker->SetExternalMovement(false);
		}
	}

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ResetToDefaultPointerInputSettings();
	}
}

void FBlendViewInputRouter::SetBlendViewEnabled(const bool bEnabled)
{
	if (bBlendViewEnabled == bEnabled)
	{
		return;
	}

	if (!bEnabled)
	{
		CancelActiveOperation();
	}

	bBlendViewEnabled = bEnabled;
	UE_LOG(LogBlendViewInput, Display, TEXT("BlendView %s"), bBlendViewEnabled ? TEXT("enabled") : TEXT("disabled"));
}

bool FBlendViewInputRouter::ExecuteCursorOriginAction(const EBlendViewCursorOriginAction Action)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!bBlendViewEnabled ||
		!Settings ||
		!Settings->bEnableSceneCursor ||
		ToolManager.HasActiveTool() ||
		GraphTransformController.IsActive() ||
		NavigationController.IsNavigating())
	{
		return false;
	}

	return FBlendViewCursorOriginActions::Execute(Action, SceneCursor);
}
