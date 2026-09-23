// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Input/BlendViewInputProcessor.h"

#include "BlendViewLog.h"
#include "BlendViewSettings.h"
#include "Core/BlendViewInputTypes.h"
#include "Core/BlendViewStateMachine.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "Input/BlendViewInputRouter.h"
#include "QuickFavorites/BlendViewQuickFavorites.h"
#include "SLevelViewport.h"
#include "Slate/SceneViewport.h"
#include "UI/SBlendViewPieMenu.h"
#include "Viewport/BlendViewViewportContext.h"

namespace
{
	bool IsHandled(const EBlendViewInputResult Result)
	{
		return Result == EBlendViewInputResult::Handled;
	}

	void CopyKeyModifiers(const FInputEvent& SourceEvent, FBlendViewInputEvent& TargetEvent)
	{
		FModifierKeysState GlobalModifierState;
		const bool bHasGlobalModifierState = FSlateApplication::IsInitialized();
		if (bHasGlobalModifierState)
		{
			GlobalModifierState = FSlateApplication::Get().GetModifierKeys();
		}

		TargetEvent.bShiftDown = SourceEvent.IsShiftDown() ||
			(bHasGlobalModifierState && GlobalModifierState.IsShiftDown());
		TargetEvent.bControlDown = SourceEvent.IsControlDown() ||
			(bHasGlobalModifierState && GlobalModifierState.IsControlDown());
		TargetEvent.bAltDown = SourceEvent.IsAltDown() ||
			(bHasGlobalModifierState && GlobalModifierState.IsAltDown());
		TargetEvent.bCommandDown = SourceEvent.IsCommandDown() ||
			(bHasGlobalModifierState && GlobalModifierState.IsCommandDown());
	}

	void CopyPointerModifiers(const FPointerEvent& SourceEvent, FBlendViewInputEvent& TargetEvent)
	{
		TargetEvent.ScreenPosition = SourceEvent.GetScreenSpacePosition();
		TargetEvent.CursorDelta = SourceEvent.GetCursorDelta();
		CopyKeyModifiers(SourceEvent, TargetEvent);
	}

	bool IsBlendViewPieTextEntryFocused()
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

	struct FBlendViewPieViewportHit
	{
		TSharedPtr<SLevelViewport> LevelViewport;

		bool IsValid() const
		{
			return LevelViewport.IsValid();
		}
	};

	FBlendViewPieViewportHit FindBlendViewPieLevelViewportUnderCursor(FSlateApplication& SlateApp)
	{
		const FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(
			SlateApp.GetCursorPos(),
			SlateApp.GetInteractiveTopLevelWindows(),
			false);
		if (!WidgetPath.IsValid())
		{
			return FBlendViewPieViewportHit();
		}

		static const FName LevelViewportType(TEXT("SLevelViewport"));
		for (int32 Index = 0; Index < WidgetPath.Widgets.Num(); ++Index)
		{
			const FArrangedWidget& ArrangedWidget = WidgetPath.Widgets[Index];
			if (ArrangedWidget.Widget->GetType() == LevelViewportType)
			{
				FBlendViewPieViewportHit Hit;
				Hit.LevelViewport = StaticCastSharedRef<SLevelViewport>(ArrangedWidget.Widget);
				return Hit;
			}
		}
		return FBlendViewPieViewportHit();
	}

	bool IsBlendViewPieModifierKey(const FKey& Key)
	{
		return Key == EKeys::LeftShift ||
			Key == EKeys::RightShift;
	}

	bool IsBlendViewPieHoldKey(const FKey& Key)
	{
		return IsBlendViewPieModifierKey(Key) ||
			Key == EKeys::S;
	}

	bool IsBlendViewPieBlockedByMouseCapture(const FSlateApplication& SlateApp)
	{
		return SlateApp.GetPressedMouseButtons().Contains(EKeys::RightMouseButton);
	}

	bool TryScreenToViewportPositionUnbounded(
		const TWeakPtr<SWidget>& ViewportWidget,
		const FIntPoint& ViewportSize,
		const FVector2D& ScreenPosition,
		FVector2D& OutViewportPosition)
	{
		const TSharedPtr<SWidget> Widget = ViewportWidget.Pin();
		if (!Widget.IsValid() || ViewportSize.X <= 0 || ViewportSize.Y <= 0)
		{
			return false;
		}

		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (LocalSize.X <= UE_KINDA_SMALL_NUMBER || LocalSize.Y <= UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector2D LocalPosition = Geometry.AbsoluteToLocal(ScreenPosition);
		OutViewportPosition = FVector2D(
			LocalPosition.X * static_cast<double>(ViewportSize.X) / LocalSize.X,
			LocalPosition.Y * static_cast<double>(ViewportSize.Y) / LocalSize.Y);
		return true;
	}
}

FBlendViewInputProcessor::FBlendViewInputProcessor()
	: InputRouter(MakeUnique<FBlendViewInputRouter>())
	, QuickFavorites(MakeUnique<FBlendViewQuickFavorites>())
{
	if (FSlateApplication::IsInitialized())
	{
		ApplicationActivationChangedHandle =
			FSlateApplication::Get().OnApplicationActivationStateChanged().AddRaw(
				this,
				&FBlendViewInputProcessor::HandleApplicationActivationChanged);
	}
}

FBlendViewInputProcessor::~FBlendViewInputProcessor()
{
	if (ApplicationActivationChangedHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(
			ApplicationActivationChangedHandle);
	}
}

void FBlendViewInputProcessor::Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (InputRouter.IsValid())
	{
		InputRouter->Tick(DeltaTime);
		if (const TOptional<EMouseCursor::Type> CursorOverride =
			InputRouter->GetHardwareCursorOverride())
		{
			Cursor->SetType(CursorOverride.GetValue());
			LastAppliedHardwareCursorOverride = CursorOverride.GetValue();
		}
		else if (LastAppliedHardwareCursorOverride.IsSet())
		{
			Cursor->SetType(EMouseCursor::Default);
			LastAppliedHardwareCursorOverride.Reset();
		}
	}
	if (!InputRouter.IsValid())
	{
		bIgnoreNextMouseMoveAfterCursorWrap = false;
		return;
	}

	const EBlendViewInteractionState State = InputRouter->GetState();
	if (State != EBlendViewInteractionState::ModalToolActive &&
		State != EBlendViewInteractionState::NavigationActive)
	{
		bIgnoreNextMouseMoveAfterCursorWrap = false;
		return;
	}

	FDisplayMetrics DisplayMetrics;
	SlateApp.GetDisplayMetrics(DisplayMetrics);
	const FPlatformRect& Bounds = DisplayMetrics.VirtualDisplayRect;
	const FVector2D CursorPosition = Cursor->GetPosition();
	FVector2D WrappedPosition = CursorPosition;
	constexpr float WrapMargin = 2.0f;
	constexpr float WrapOffset = 24.0f;

	if (CursorPosition.X <= Bounds.Left + WrapMargin)
	{
		WrappedPosition.X = Bounds.Right - WrapOffset;
	}
	else if (CursorPosition.X >= Bounds.Right - WrapMargin)
	{
		WrappedPosition.X = Bounds.Left + WrapOffset;
	}

	if (CursorPosition.Y <= Bounds.Top + WrapMargin)
	{
		WrappedPosition.Y = Bounds.Bottom - WrapOffset;
	}
	else if (CursorPosition.Y >= Bounds.Bottom - WrapMargin)
	{
		WrappedPosition.Y = Bounds.Top + WrapOffset;
	}

	if (!WrappedPosition.Equals(CursorPosition, 0.5f))
	{
		Cursor->SetPosition(
			FMath::RoundToInt(WrappedPosition.X),
			FMath::RoundToInt(WrappedPosition.Y));
		bIgnoreNextMouseMoveAfterCursorWrap = true;
	}
}

bool FBlendViewInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (TryOpenQuickFavorites(SlateApp, InKeyEvent))
	{
		return true;
	}

	if (QuickFavorites.IsValid() && QuickFavorites->TryOpenSearch(SlateApp, InKeyEvent))
	{
		return true;
	}

	if (TryTogglePieMenu(SlateApp, InKeyEvent))
	{
		return true;
	}

	if (IsPieMenuOpen() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		ClosePieMenu();
		return true;
	}

	if (IsPieMenuOpen() && IsBlendViewPieHoldKey(InKeyEvent.GetKey()))
	{
		return true;
	}

	if (InputRouter.IsValid() && InputRouter->RouteFlightNavigationKeyDown(InKeyEvent))
	{
		return true;
	}

	FBlendViewInputEvent Event;
	Event.Type = EBlendViewInputEventType::KeyDown;
	Event.Key = InKeyEvent.GetKey();
	Event.bIsRepeat = InKeyEvent.IsRepeat();
	CopyKeyModifiers(InKeyEvent, Event);
	return InputRouter.IsValid() && IsHandled(InputRouter->RouteInput(Event));
}

bool FBlendViewInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsPieMenuOpen() && IsBlendViewPieModifierKey(InKeyEvent.GetKey()))
	{
		if (PieMenuWidget.IsValid())
		{
			EBlendViewCursorOriginAction Action = EBlendViewCursorOriginAction::CursorToOrigin;
			FVector2D ViewportPosition = PieMenuViewportPosition;
			TryScreenToViewportPositionUnbounded(
				PieMenuViewportWidget,
				PieMenuViewportSize,
				SlateApp.GetCursorPos(),
				ViewportPosition);
			if (PieMenuWidget->TryGetHoveredAction(Action) ||
				PieMenuWidget->TryGetActionAtViewportPosition(ViewportPosition, Action))
			{
				UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Shift released, executing hovered action: %s"), FBlendViewCursorOriginActions::ToLogName(Action));
				ExecutePieMenuAction(Action);
			}
			else
			{
				UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Shift released, no hovered pie action."));
			}
		}
		ClosePieMenu();
		return true;
	}

	if (IsPieMenuOpen() && InKeyEvent.GetKey() == EKeys::S)
	{
		return true;
	}

	if (InputRouter.IsValid() && InputRouter->RouteFlightNavigationKeyUp(InKeyEvent))
	{
		return true;
	}

	FBlendViewInputEvent Event;
	Event.Type = EBlendViewInputEventType::KeyUp;
	Event.Key = InKeyEvent.GetKey();
	CopyKeyModifiers(InKeyEvent, Event);
	return InputRouter.IsValid() && IsHandled(InputRouter->RouteInput(Event));
}

bool FBlendViewInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	UpdatePieMenuPointer(MouseEvent.GetScreenSpacePosition());

	FBlendViewInputEvent Event;
	Event.Type = EBlendViewInputEventType::MouseMove;
	CopyPointerModifiers(MouseEvent, Event);
	if (bIgnoreNextMouseMoveAfterCursorWrap)
	{
		Event.CursorDelta = FVector2D::ZeroVector;
		bIgnoreNextMouseMoveAfterCursorWrap = false;
	}
	return InputRouter.IsValid() && IsHandled(InputRouter->RouteInput(Event));
}

bool FBlendViewInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (IsPieMenuOpen())
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && PieMenuWidget.IsValid())
		{
			EBlendViewCursorOriginAction Action = EBlendViewCursorOriginAction::CursorToOrigin;
			FVector2D ViewportPosition = PieMenuViewportPosition;
			TryScreenToViewportPositionUnbounded(
				PieMenuViewportWidget,
				PieMenuViewportSize,
				MouseEvent.GetScreenSpacePosition(),
				ViewportPosition);
			if (PieMenuWidget->TryGetActionAtViewportPosition(ViewportPosition, Action))
			{
				UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Left click hit pie action: %s"), FBlendViewCursorOriginActions::ToLogName(Action));
				ExecutePieMenuAction(Action);
			}
			else
			{
				UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Left click missed pie action at screen position: X=%.1f Y=%.1f"),
					MouseEvent.GetScreenSpacePosition().X,
					MouseEvent.GetScreenSpacePosition().Y);
			}
		}
		else
		{
			UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Pie menu closed by mouse button: %s"), *MouseEvent.GetEffectingButton().ToString());
		}
		ClosePieMenu();
		return true;
	}

	FBlendViewInputEvent Event;
	Event.Type = EBlendViewInputEventType::MouseDown;
	Event.Key = MouseEvent.GetEffectingButton();
	CopyPointerModifiers(MouseEvent, Event);
	return InputRouter.IsValid() && IsHandled(InputRouter->RouteInput(Event));
}

bool FBlendViewInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	FBlendViewInputEvent Event;
	Event.Type = EBlendViewInputEventType::MouseUp;
	Event.Key = MouseEvent.GetEffectingButton();
	CopyPointerModifiers(MouseEvent, Event);
	return InputRouter.IsValid() && IsHandled(InputRouter->RouteInput(Event));
}

bool FBlendViewInputProcessor::HandleMouseButtonDoubleClickEvent(
	FSlateApplication& SlateApp,
	const FPointerEvent& MouseEvent)
{
	FBlendViewInputEvent Event;
	Event.Type = EBlendViewInputEventType::MouseDoubleClick;
	Event.Key = MouseEvent.GetEffectingButton();
	CopyPointerModifiers(MouseEvent, Event);
	return InputRouter.IsValid() && IsHandled(InputRouter->RouteInput(Event));
}

bool FBlendViewInputProcessor::HandleMouseWheelOrGestureEvent(
	FSlateApplication& SlateApp,
	const FPointerEvent& InWheelEvent,
	const FPointerEvent* InGestureEvent)
{
	FBlendViewInputEvent Event;
	Event.Type = EBlendViewInputEventType::MouseWheel;
	Event.Key = InWheelEvent.GetEffectingButton();
	CopyPointerModifiers(InWheelEvent, Event);
	return InputRouter.IsValid() && IsHandled(InputRouter->RouteInput(Event));
}

void FBlendViewInputProcessor::CancelActiveOperation()
{
	ClosePieMenu();
	if (InputRouter.IsValid())
	{
		InputRouter->CancelActiveOperation();
	}
}

void FBlendViewInputProcessor::PrepareForEngineExit()
{
	ClosePieMenu();
	if (InputRouter.IsValid())
	{
		InputRouter->PrepareForEngineExit();
	}
}

void FBlendViewInputProcessor::DrawHUD(UCanvas* Canvas)
{
	if (InputRouter.IsValid())
	{
		InputRouter->DrawHUD(Canvas);
	}
}

void FBlendViewInputProcessor::SetBlendViewEnabled(const bool bEnabled)
{
	if (InputRouter.IsValid())
	{
		InputRouter->SetBlendViewEnabled(bEnabled);
	}
}

bool FBlendViewInputProcessor::IsBlendViewEnabled() const
{
	return InputRouter.IsValid() && InputRouter->IsBlendViewEnabled();
}

EBlendViewInteractionState FBlendViewInputProcessor::GetState() const
{
	return InputRouter.IsValid()
		? InputRouter->GetState()
		: EBlendViewInteractionState::Idle;
}

void FBlendViewInputProcessor::HandleApplicationActivationChanged(const bool bIsActive)
{
	if (!bIsActive)
	{
		ClosePieMenu();
		bIgnoreNextMouseMoveAfterCursorWrap = false;
		CancelActiveOperation();
	}
}

bool FBlendViewInputProcessor::TryTogglePieMenu(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const bool bIsShiftS =
		InKeyEvent.GetKey() == EKeys::S &&
		InKeyEvent.IsShiftDown() &&
		!InKeyEvent.IsControlDown() &&
		!InKeyEvent.IsAltDown() &&
		!InKeyEvent.IsCommandDown();
	if (!bIsShiftS || InKeyEvent.IsRepeat())
	{
		return false;
	}

	if (IsPieMenuOpen())
	{
		return true;
	}

	if (IsBlendViewPieBlockedByMouseCapture(SlateApp))
	{
		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Shift+S pie menu blocked because right mouse button is down."));
		return false;
	}

	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!InputRouter.IsValid() ||
		!InputRouter->IsBlendViewEnabled() ||
		InputRouter->GetState() != EBlendViewInteractionState::Idle ||
		!Settings ||
		!Settings->bEnablePieMenu ||
		IsBlendViewPieTextEntryFocused() ||
		!FindBlendViewPieLevelViewportUnderCursor(SlateApp).IsValid())
	{
		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Shift+S pie menu rejected. Router=%d Enabled=%d State=%d Settings=%d PieEnabled=%d TextFocused=%d ViewportUnderCursor=%d"),
			InputRouter.IsValid() ? 1 : 0,
			InputRouter.IsValid() && InputRouter->IsBlendViewEnabled() ? 1 : 0,
			InputRouter.IsValid() ? static_cast<int32>(InputRouter->GetState()) : -1,
			Settings ? 1 : 0,
			Settings && Settings->bEnablePieMenu ? 1 : 0,
			IsBlendViewPieTextEntryFocused() ? 1 : 0,
			FindBlendViewPieLevelViewportUnderCursor(SlateApp).IsValid() ? 1 : 0);
		return false;
	}

	OpenPieMenu(SlateApp);
	return true;
}

bool FBlendViewInputProcessor::TryOpenQuickFavorites(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	return QuickFavorites.IsValid() && QuickFavorites->TryOpen(SlateApp, InKeyEvent);
}

void FBlendViewInputProcessor::OpenPieMenu(FSlateApplication& SlateApp)
{
	ClosePieMenu();

	const FBlendViewPieViewportHit ViewportHit = FindBlendViewPieLevelViewportUnderCursor(SlateApp);
	const TSharedPtr<SLevelViewport> LevelViewport = ViewportHit.LevelViewport;
	if (!LevelViewport.IsValid())
	{
		return;
	}

	PieMenuScreenPosition = SlateApp.GetCursorPos();
	const FBlendViewViewportResolver ViewportResolver;
	const FBlendViewViewportContext ViewportContext =
		ViewportResolver.ResolveForMousePosition(PieMenuScreenPosition);
	if (!ViewportContext.IsValid() ||
		ViewportContext.Kind != EBlendViewViewportKind::LevelEditor ||
		ViewportContext.ViewportSize.X <= 0 ||
		ViewportContext.ViewportSize.Y <= 0)
	{
		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Shift+S pie menu rejected because viewport context was invalid."));
		return;
	}

	PieMenuViewportPosition = ViewportContext.MouseViewportPosition;
	PieMenuViewportSize = ViewportContext.ViewportSize;
	PieMenuViewportWidget = ViewportContext.ViewportWidget;
	UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Opening Shift+S pie menu at screen position: X=%.1f Y=%.1f, viewport position: X=%.1f Y=%.1f, viewport size: X=%d Y=%d"),
		PieMenuScreenPosition.X,
		PieMenuScreenPosition.Y,
		PieMenuViewportPosition.X,
		PieMenuViewportPosition.Y,
		PieMenuViewportSize.X,
		PieMenuViewportSize.Y);
	SAssignNew(PieMenuWidget, SBlendViewPieMenu)
		.OnRequestClose(FSimpleDelegate::CreateRaw(this, &FBlendViewInputProcessor::ClosePieMenu))
		.OnAction(FBlendViewPieMenuActionDelegate::CreateRaw(this, &FBlendViewInputProcessor::ExecutePieMenuAction));
	PieMenuWidget->SetAnchorViewportPosition(PieMenuViewportPosition, PieMenuViewportSize);
	PieMenuWidget->SetPointerViewportPosition(PieMenuViewportPosition);
	PieMenuLevelViewport = LevelViewport;
	LevelViewport->AddOverlayWidget(PieMenuWidget.ToSharedRef(), 10000);
}

void FBlendViewInputProcessor::ClosePieMenu()
{
	if (PieMenuWidget.IsValid())
	{
		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Closing Shift+S pie menu."));
	}
	ReleasePieMenuNavigationKeys();
	if (const TSharedPtr<SLevelViewport> LevelViewport = PieMenuLevelViewport.Pin())
	{
		if (PieMenuWidget.IsValid())
		{
			LevelViewport->RemoveOverlayWidget(PieMenuWidget.ToSharedRef());
		}
	}
	PieMenuWidget.Reset();
	PieMenuLevelViewport.Reset();
	PieMenuViewportWidget.Reset();
	PieMenuScreenPosition = FVector2D::ZeroVector;
	PieMenuViewportPosition = FVector2D::ZeroVector;
	PieMenuViewportSize = FIntPoint::ZeroValue;
}

void FBlendViewInputProcessor::ExecutePieMenuAction(const EBlendViewCursorOriginAction Action)
{
	if (InputRouter.IsValid())
	{
		const bool bExecuted = InputRouter->ExecuteCursorOriginAction(Action);
		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("Pie action %s execution result: %s"),
			FBlendViewCursorOriginActions::ToLogName(Action),
			bExecuted ? TEXT("Success") : TEXT("Failed"));
	}
	else
	{
		UE_LOG(LogBlendViewPieMenu, Warning, TEXT("Pie action %s failed because InputRouter is invalid."),
			FBlendViewCursorOriginActions::ToLogName(Action));
	}
}

void FBlendViewInputProcessor::ReleasePieMenuNavigationKeys()
{
	const TSharedPtr<SLevelViewport> LevelViewport = PieMenuLevelViewport.Pin();
	if (!LevelViewport.IsValid())
	{
		return;
	}

	FSceneViewport* Viewport = static_cast<FSceneViewport*>(LevelViewport->GetActiveViewport());
	if (!Viewport || !Viewport->KeyState(EKeys::S))
	{
		return;
	}

	Viewport->OnKeyUp(
		FGeometry(),
		FKeyEvent(EKeys::S, FModifierKeysState(), 0, false, 0, 0));
}

bool FBlendViewInputProcessor::IsPieMenuOpen() const
{
	return PieMenuWidget.IsValid();
}

void FBlendViewInputProcessor::UpdatePieMenuPointer(const FVector2D& ScreenPosition)
{
	if (!PieMenuWidget.IsValid())
	{
		return;
	}

	FVector2D ViewportPosition = PieMenuViewportPosition;
	if (TryScreenToViewportPositionUnbounded(
			PieMenuViewportWidget,
			PieMenuViewportSize,
			ScreenPosition,
			ViewportPosition))
	{
		PieMenuViewportPosition = ViewportPosition;
		PieMenuWidget->SetPointerViewportPosition(PieMenuViewportPosition);
	}
}
