// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Viewport/BlendViewViewportProjector.h"

#include "Compat/BlendViewViewportCompat.h"
#include "EditorViewportClient.h"
#include "Engine/Canvas.h"
#include "SceneView.h"
#include "Viewport/BlendViewViewportContext.h"

FSceneView* FBlendViewViewportProjector::CreateSceneView(
	const FBlendViewViewportContext& Context,
	FSceneViewFamilyContext& ViewFamily)
{
	return Context.IsValid()
		? Context.ViewportClient->CalcSceneView(&ViewFamily)
		: nullptr;
}

bool FBlendViewViewportProjector::ProjectWorldToViewport(
	const FBlendViewViewportContext& Context,
	const FVector& WorldPosition,
	FVector2D& OutViewportPosition)
{
	if (!Context.IsValid())
	{
		return false;
	}
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Context.Viewport,
		Context.ViewportClient->GetScene(),
		Context.ViewportClient->EngineShowFlags));
	const FSceneView* View = CreateSceneView(Context, ViewFamily);
	if (!View || !View->WorldToPixel(WorldPosition, OutViewportPosition))
	{
		return false;
	}
	OutViewportPosition -= FVector2D(View->UnscaledViewRect.Min.X, View->UnscaledViewRect.Min.Y);
	return true;
}

bool FBlendViewViewportProjector::GetViewportRay(
	const FBlendViewViewportContext& Context,
	const FVector2D& ViewportPosition,
	FVector& OutRayStart,
	FVector& OutRayDirection)
{
	if (!Context.IsValid())
	{
		return false;
	}
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Context.Viewport,
		Context.ViewportClient->GetScene(),
		Context.ViewportClient->EngineShowFlags));
	FSceneView* View = CreateSceneView(Context, ViewFamily);
	if (!View)
	{
		return false;
	}
	const FVector2D ScenePixel = ViewportPosition + FVector2D(
		View->UnscaledViewRect.Min.X,
		View->UnscaledViewRect.Min.Y);
	View->DeprojectFVector2D(ScenePixel, OutRayStart, OutRayDirection);
	OutRayDirection.Normalize();
	return !OutRayDirection.IsNearlyZero();
}

bool FBlendViewViewportProjector::GetViewportTraceSegment(
	const FBlendViewViewportContext& Context,
	const FVector2D& ViewportPosition,
	FVector& OutTraceStart,
	FVector& OutTraceEnd)
{
	if (!Context.IsValid())
	{
		return false;
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Context.Viewport,
		Context.ViewportClient->GetScene(),
		Context.ViewportClient->EngineShowFlags));
	const FSceneView* View = CreateSceneView(Context, ViewFamily);
	if (!View)
	{
		return false;
	}

	const FVector2D ScenePixel = ViewportPosition + FVector2D(
		View->UnscaledViewRect.Min.X,
		View->UnscaledViewRect.Min.Y);
	const FViewportCursorLocation Cursor = BlendViewViewportCompat::MakeCursorLocation(
		View,
		Context.ViewportClient,
		FMath::RoundToInt(ScenePixel.X),
		FMath::RoundToInt(ScenePixel.Y),
		false);
	const FVector TraceDirection = Cursor.GetDirection().GetSafeNormal();
	if (TraceDirection.IsNearlyZero())
	{
		return false;
	}

	OutTraceStart = Cursor.GetOrigin();
	if (Context.ViewportClient->IsPerspective())
	{
		OutTraceEnd = OutTraceStart + TraceDirection * 1000000.0;
	}
	else
	{
		OutTraceStart -= TraceDirection * (HALF_WORLD_MAX * 0.5);
		OutTraceEnd = OutTraceStart + TraceDirection * HALF_WORLD_MAX;
	}
	return true;
}

bool FBlendViewViewportProjector::GetViewBasis(
	const FBlendViewViewportContext& Context,
	FVector& OutViewForward,
	FVector& OutViewRight,
	FVector& OutViewUp)
{
	if (!Context.IsValid())
	{
		return false;
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Context.Viewport,
		Context.ViewportClient->GetScene(),
		Context.ViewportClient->EngineShowFlags));
	const FSceneView* View = CreateSceneView(Context, ViewFamily);
	if (!View)
	{
		return false;
	}

	OutViewForward = BlendViewViewportCompat::GetForwardVector(*Context.ViewportClient);
	OutViewRight = View->GetViewRight().GetSafeNormal();
	OutViewUp = View->GetViewUp().GetSafeNormal();
	return !OutViewForward.IsNearlyZero() &&
		!OutViewRight.IsNearlyZero() &&
		!OutViewUp.IsNearlyZero();
}

bool FBlendViewViewportProjector::ProjectWorldToCanvas(
	UCanvas* Canvas,
	const FVector& WorldPosition,
	FVector2D& OutCanvasPosition)
{
	if (!Canvas || !Canvas->SceneView)
	{
		return false;
	}
	const FVector ProjectedPosition = Canvas->Project(WorldPosition, false);
	if (ProjectedPosition.Z < 0.0)
	{
		return false;
	}
	OutCanvasPosition = FVector2D(ProjectedPosition.X, ProjectedPosition.Y);
	return true;
}

bool FBlendViewViewportProjector::IsCanvasForViewport(
	const FBlendViewViewportContext& Context,
	UCanvas* Canvas)
{
	return Context.IsValid() &&
		Canvas &&
		Canvas->Canvas &&
		Canvas->SceneView &&
		Canvas->SceneView->Family &&
		Canvas->SceneView->Family->RenderTarget == Context.Viewport;
}

bool FBlendViewViewportProjector::ProjectWorldToOverlay(
	const FBlendViewViewportContext& Context,
	const FVector& WorldPosition,
	FVector2D& OutNormalizedPosition)
{
	FVector2D ViewportPosition;
	const FIntPoint ViewportSize = Context.IsValid()
		? Context.Viewport->GetSizeXY()
		: FIntPoint::ZeroValue;
	if (!ProjectWorldToViewport(Context, WorldPosition, ViewportPosition) ||
		ViewportSize.X <= 0 ||
		ViewportSize.Y <= 0)
	{
		return false;
	}
	OutNormalizedPosition = FVector2D(
		ViewportPosition.X / static_cast<double>(ViewportSize.X),
		ViewportPosition.Y / static_cast<double>(ViewportSize.Y));
	return true;
}
