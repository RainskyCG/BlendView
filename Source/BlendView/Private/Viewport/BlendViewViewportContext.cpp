// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Viewport/BlendViewViewportContext.h"

#include "Editor.h"
#include "EditorViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "ILevelEditor.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Modules/ModuleManager.h"
#include "SEditorViewport.h"
#include "Slate/SceneViewport.h"
#include "SLevelViewport.h"
#include "UnrealClient.h"
#include "Viewport/BlendViewViewportGeometry.h"
#include "Widgets/SViewport.h"

namespace
{
	bool IsLevelEditorViewportClient(const FEditorViewportClient* ViewportClient)
	{
		if (!GEditor || !ViewportClient)
		{
			return false;
		}

		for (const FLevelEditorViewportClient* LevelViewportClient : GEditor->GetLevelViewportClients())
		{
			if (LevelViewportClient == ViewportClient)
			{
				return true;
			}
		}

		return false;
	}

	bool TryFillContextFromViewport(
		FBlendViewViewportContext& Context,
		FEditorViewportClient* ViewportClient,
		FSceneViewport* Viewport,
		const TSharedPtr<SWidget>& ViewportWidget,
		const FVector2D& ScreenPosition,
		const FIntPoint& ScreenPixel,
		const bool bRequireVisible = false)
	{
		if (!ViewportClient || !Viewport || (bRequireVisible && !ViewportClient->IsVisible()))
		{
			return false;
		}

		const FIntPoint ViewportSize = Viewport->GetSizeXY();
		FVector2D MouseViewportPosition = FVector2D::ZeroVector;
		bool bHasMouseViewportPosition = false;
		if (FBlendViewViewportGeometry::TryScreenToViewportPosition(
				ViewportWidget,
				ScreenPosition,
				ViewportSize,
				MouseViewportPosition))
		{
			bHasMouseViewportPosition = true;
		}

		if (!bHasMouseViewportPosition)
		{
			const FVector2D NormalizedPosition = Viewport->VirtualDesktopPixelToViewport(ScreenPixel);
			if (NormalizedPosition.X < 0.0 || NormalizedPosition.X > 1.0 ||
				NormalizedPosition.Y < 0.0 || NormalizedPosition.Y > 1.0)
			{
				return false;
			}
			MouseViewportPosition = FVector2D(ViewportSize) * NormalizedPosition;
		}

		Context.ViewportClient = ViewportClient;
		Context.Viewport = Viewport;
		Context.SceneViewport = Viewport;
		Context.ViewportWidget = ViewportWidget;
		Context.ViewportSize = ViewportSize;
		Context.Kind = IsLevelEditorViewportClient(ViewportClient)
			? EBlendViewViewportKind::LevelEditor
			: EBlendViewViewportKind::EditorViewport;
		Context.MouseViewportPosition = MouseViewportPosition;
		return true;
	}


}

FBlendViewViewportContext FBlendViewViewportResolver::ResolveForMousePosition(const FVector2D& ScreenPosition) const
{
	FBlendViewViewportContext Context;
	Context.MouseScreenPosition = ScreenPosition;

	if (!GEditor || !FSlateApplication::IsInitialized())
	{
		return Context;
	}

	const FWidgetPath WidgetPath = FSlateApplication::Get().LocateWindowUnderMouse(
		ScreenPosition,
		FSlateApplication::Get().GetInteractiveTopLevelWindows(),
		false);
	if (!WidgetPath.IsValid())
	{
		return Context;
	}

	const FIntPoint ScreenPixel(
		FMath::RoundToInt(ScreenPosition.X),
		FMath::RoundToInt(ScreenPosition.Y));
	TSharedPtr<SViewport> SlateViewportWidgetUnderMouse;
	TSharedPtr<SWidget> GeometryWidgetUnderMouse;
	for (int32 Index = WidgetPath.Widgets.Num() - 1; Index >= 0; --Index)
	{
		const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[Index].Widget;
		if (Widget->GetType() == TEXT("SViewport"))
		{
			SlateViewportWidgetUnderMouse = StaticCastSharedRef<SViewport>(Widget);
			GeometryWidgetUnderMouse = Widget;
			break;
		}
	}

	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LevelEditorModule =
			FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		const TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
		if (LevelEditor.IsValid())
		{
			for (const TSharedPtr<SLevelViewport>& LevelViewport : LevelEditor->GetViewports())
			{
				if (!LevelViewport.IsValid() || !WidgetPath.ContainsWidget(LevelViewport.Get()))
				{
					continue;
				}

				FLevelEditorViewportClient* ViewportClient = &LevelViewport->GetLevelViewportClient();
				const TSharedPtr<FSceneViewport> Viewport = LevelViewport->GetSceneViewport();
				const TSharedPtr<SWidget> GeometryWidget = GeometryWidgetUnderMouse.IsValid()
					? GeometryWidgetUnderMouse
					: StaticCastSharedPtr<SWidget>(LevelViewport);
				if (!TryFillContextFromViewport(
					Context,
					ViewportClient,
					Viewport.Get(),
					GeometryWidget,
					ScreenPosition,
					ScreenPixel,
					true))
				{
					return Context;
				}
				return Context;
			}
		}
	}

	if (!SlateViewportWidgetUnderMouse.IsValid())
	{
		return Context;
	}

	const TSharedPtr<ISlateViewport> SlateViewport = SlateViewportWidgetUnderMouse->GetViewportInterface().Pin();
	if (!SlateViewport.IsValid())
	{
		return Context;
	}

	for (FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
	{
		if (!ViewportClient)
		{
			continue;
		}

		const TSharedPtr<SEditorViewport> EditorViewport = ViewportClient->GetEditorViewportWidget();
		const TSharedPtr<FSceneViewport> SceneViewport = EditorViewport.IsValid()
			? EditorViewport->GetSceneViewport()
			: nullptr;
		if (!SceneViewport.IsValid() ||
			SceneViewport->GetViewportWidget().Pin().Get() != SlateViewportWidgetUnderMouse.Get())
		{
			continue;
		}

		if (!TryFillContextFromViewport(
			Context,
			ViewportClient,
			SceneViewport.Get(),
			GeometryWidgetUnderMouse,
			ScreenPosition,
			ScreenPixel))
		{
			return Context;
		}
		return Context;
	}

	return Context;
}
