// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Cursor/BlendViewSceneCursorPlacement.h"

#include "Cursor/BlendViewSceneCursor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "Viewport/BlendViewViewportContext.h"
#include "Viewport/BlendViewViewportProjector.h"

namespace
{
	constexpr double BlendViewCursorFallbackDepth = 1000.0;
	constexpr double BlendViewCursorTraceDistance = 1000000.0;

	FVector ProjectOntoPlane(const FVector& Direction, const FVector& PlaneNormal)
	{
		return Direction - PlaneNormal * FVector::DotProduct(Direction, PlaneNormal);
	}

	FQuat GetPlacementRotation(const FBlendViewSceneCursor& CurrentCursor)
	{
		return CurrentCursor.IsVisible()
			? CurrentCursor.GetRotation()
			: FQuat::Identity;
	}

	bool BuildPlacementTransform(
		const FQuat& Rotation,
		const FVector& Location,
		const bool bUpdatedRotation,
		FBlendViewSceneCursorPlacementResult& OutResult)
	{
		if (Location.ContainsNaN())
		{
			return false;
		}

		OutResult.Transform = FTransform(
			Rotation,
			Location,
			FVector::OneVector);
		OutResult.bUpdatedRotation = bUpdatedRotation;
		return true;
	}

	bool TryNormalize(FVector& Vector)
	{
		return Vector.Normalize(UE_SMALL_NUMBER);
	}

	FVector BuildFallbackXAxis(const FVector& ZAxis, const FVector& ViewRight, const FVector& ViewUp)
	{
		FVector XAxis = ProjectOntoPlane(ViewRight, ZAxis);
		if (TryNormalize(XAxis))
		{
			return XAxis;
		}

		XAxis = ProjectOntoPlane(ViewUp, ZAxis);
		if (TryNormalize(XAxis))
		{
			return XAxis;
		}

		XAxis = FVector::CrossProduct(FVector::UpVector, ZAxis);
		if (TryNormalize(XAxis))
		{
			return XAxis;
		}

		return FVector::CrossProduct(FVector::RightVector, ZAxis).GetSafeNormal();
	}

	FQuat BuildViewRotation(const FVector& ViewRight, const FVector& ViewUp)
	{
		return FRotationMatrix::MakeFromXY(
			ViewRight.GetSafeNormal(),
			ViewUp.GetSafeNormal()).ToQuat();
	}

	FQuat BuildSurfaceRotation(
		const FVector& SurfaceNormal,
		const FVector& ViewRight,
		const FVector& ViewUp,
		const FBlendViewSceneCursor& CurrentCursor)
	{
		const FVector ZAxis = SurfaceNormal.GetSafeNormal();
		if (ZAxis.IsNearlyZero())
		{
			return GetPlacementRotation(CurrentCursor);
		}

		FVector XAxis = FVector::ZeroVector;
		if (CurrentCursor.IsVisible())
		{
			const FQuat CurrentRotation = GetPlacementRotation(CurrentCursor);
			XAxis = ProjectOntoPlane(CurrentRotation.GetAxisX(), ZAxis);
			if (!TryNormalize(XAxis))
			{
				XAxis = ProjectOntoPlane(CurrentRotation.GetAxisY(), ZAxis);
				TryNormalize(XAxis);
			}
		}

		if (XAxis.IsNearlyZero())
		{
			XAxis = BuildFallbackXAxis(ZAxis, ViewRight, ViewUp);
		}

		if (CurrentCursor.IsVisible())
		{
			const FVector PreviousX = GetPlacementRotation(CurrentCursor).GetAxisX();
			if (FVector::DotProduct(XAxis, PreviousX) < 0.0)
			{
				XAxis *= -1.0;
			}
		}

		return FRotationMatrix::MakeFromXZ(XAxis, ZAxis).ToQuat();
	}

	FQuat ResolvePlacementRotation(
		const EBlendViewSceneCursorOrientationMode OrientationMode,
		const FBlendViewSceneCursor& CurrentCursor,
		const FVector& ViewRight,
		const FVector& ViewUp,
		const FVector* SurfaceNormal,
		bool& bOutUpdatedRotation)
	{
		bOutUpdatedRotation = false;

		switch (OrientationMode)
		{
		case EBlendViewSceneCursorOrientationMode::World:
			bOutUpdatedRotation = true;
			return FQuat::Identity;
		case EBlendViewSceneCursorOrientationMode::View:
			bOutUpdatedRotation = true;
			return BuildViewRotation(ViewRight, ViewUp);
		case EBlendViewSceneCursorOrientationMode::Surface:
			if (SurfaceNormal)
			{
				bOutUpdatedRotation = true;
				return BuildSurfaceRotation(*SurfaceNormal, ViewRight, ViewUp, CurrentCursor);
			}
			break;
		default:
			break;
		}

		return GetPlacementRotation(CurrentCursor);
	}

	bool TryTraceSurface(
		const FBlendViewViewportContext& Context,
		const FVector2D& ViewportPosition,
		FHitResult& OutHit)
	{
		UWorld* World = Context.ViewportClient ? Context.ViewportClient->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}

		FVector TraceStart;
		FVector TraceEnd;
		if (!FBlendViewViewportProjector::GetViewportTraceSegment(
				Context,
				ViewportPosition,
				TraceStart,
				TraceEnd))
		{
			return false;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlendViewCursorPlacement), true);
		return World->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) &&
			OutHit.IsValidBlockingHit();
	}

	bool ResolveFallbackLocation(
		const FBlendViewViewportContext& Context,
		const FVector2D& ViewportPosition,
		const FBlendViewSceneCursor& CurrentCursor,
		const FVector& ViewForward,
		FVector& OutLocation)
	{
		FVector RayStart;
		FVector RayDirection;
		if (!FBlendViewViewportProjector::GetViewportRay(Context, ViewportPosition, RayStart, RayDirection))
		{
			return false;
		}

		const FVector PlaneNormal = ViewForward.GetSafeNormal();
		if (PlaneNormal.IsNearlyZero() ||
			FMath::IsNearlyZero(FVector::DotProduct(RayDirection, PlaneNormal)))
		{
			OutLocation = RayStart + RayDirection * BlendViewCursorFallbackDepth;
			return true;
		}

		const FVector PlaneOrigin = CurrentCursor.IsVisible()
			? CurrentCursor.GetLocation()
			: RayStart + RayDirection * BlendViewCursorFallbackDepth;
		const FVector RayEnd = RayStart + RayDirection * BlendViewCursorTraceDistance;
		OutLocation = FMath::LinePlaneIntersection(RayStart, RayEnd, PlaneOrigin, PlaneNormal);
		return true;
	}
}

bool FBlendViewSceneCursorPlacement::Resolve(
	const FBlendViewViewportContext& Context,
	const FVector2D& ViewportPosition,
	const FBlendViewSceneCursor& CurrentCursor,
	const FBlendViewSceneCursorPlacementOptions& Options,
	FBlendViewSceneCursorPlacementResult& OutResult)
{
	if (!Context.IsValid() || !Context.ViewportClient)
	{
		return false;
	}

	FVector ViewForward;
	FVector ViewRight;
	FVector ViewUp;
	if (!FBlendViewViewportProjector::GetViewBasis(Context, ViewForward, ViewRight, ViewUp))
	{
		return false;
	}

	FHitResult Hit;
	if (Options.bSurfaceProject && TryTraceSurface(Context, ViewportPosition, Hit))
	{
		bool bUpdatedRotation = false;
		const FQuat Rotation = ResolvePlacementRotation(
			Options.OrientationMode,
			CurrentCursor,
			ViewRight,
			ViewUp,
			&Hit.ImpactNormal,
			bUpdatedRotation);
		if (BuildPlacementTransform(Rotation, Hit.ImpactPoint, bUpdatedRotation, OutResult))
		{
			OutResult.bHitSurface = true;
			return true;
		}
	}

	FVector Location;
	if (!ResolveFallbackLocation(Context, ViewportPosition, CurrentCursor, ViewForward, Location))
	{
		return false;
	}

	bool bUpdatedRotation = false;
	const FQuat Rotation = ResolvePlacementRotation(
		Options.OrientationMode,
		CurrentCursor,
		ViewRight,
		ViewUp,
		nullptr,
		bUpdatedRotation);
	return BuildPlacementTransform(Rotation, Location, bUpdatedRotation, OutResult);
}
