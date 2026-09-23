// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Cursor/BlendViewCursorOriginActions.h"

#include "BlendViewLog.h"
#include "Core/BlendViewSessionState.h"
#include "Cursor/BlendViewSceneCursor.h"
#include "Cursor/BlendViewSceneCursorState.h"
#include "Cursor/BlendViewCursorOriginUtils.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"

namespace
{
	using namespace BlendViewCursorOriginUtils;

	bool TryGetCursorTransform(const FBlendViewSceneCursor& SceneCursor, FTransform& OutTransform)
	{
		return FBlendViewSceneCursorState::TryGetTransform(SceneCursor, OutTransform);
	}

	void SetCursorLocation(FBlendViewSceneCursor& SceneCursor, const FVector& Location)
	{
		FBlendViewSceneCursorState::SetLocationPreservingRotation(SceneCursor, Location);
		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("3D Cursor location set to: X=%.3f Y=%.3f Z=%.3f"),
			Location.X,
			Location.Y,
			Location.Z);
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
	}

	void SetCursorTransform(FBlendViewSceneCursor& SceneCursor, const FTransform& Transform)
	{
		FBlendViewSceneCursorState::SetTransform(SceneCursor, Transform);
		const FRotator Rotation = Transform.GetRotation().Rotator();
		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("3D Cursor transform set. Location=(%.3f, %.3f, %.3f) Rotation=(%.3f, %.3f, %.3f)"),
			Transform.GetLocation().X,
			Transform.GetLocation().Y,
			Transform.GetLocation().Z,
			Rotation.Roll,
			Rotation.Pitch,
			Rotation.Yaw);
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
	}

	bool MoveSelectionToCursor(const bool bKeepOffset, FBlendViewSceneCursor& SceneCursor)
	{
		FTransform CursorTransform;
		if (!TryGetCursorTransform(SceneCursor, CursorTransform))
		{
			UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("SelectionToCursor failed: no 3D cursor is available."));
			return false;
		}

		TArray<AActor*> Actors;
		GetSelectedEditableActors(Actors);
		if (Actors.IsEmpty())
		{
			UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("SelectionToCursor failed: no editable selected actors."));
			return false;
		}

		FVector Delta = FVector::ZeroVector;
		if (bKeepOffset)
		{
			if (Actors.Num() == 1)
			{
				Delta = CursorTransform.GetLocation() - GetActorPivotWorldLocation(*Actors[0]);
			}
			else
			{
				FBox SelectionBounds;
				if (!GetSelectionBounds(Actors, SelectionBounds))
				{
					UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("SelectionToCursorOffset failed: invalid selection bounds."));
					return false;
				}
				Delta = CursorTransform.GetLocation() - SelectionBounds.GetCenter();
			}
		}

		const FScopedTransaction Transaction(
			bKeepOffset
				? NSLOCTEXT("BlendView", "MoveSelectionToCursorOffset", "BlendView Move Selection to Cursor Offset")
				: NSLOCTEXT("BlendView", "MoveSelectionToCursor", "BlendView Move Selection to Cursor"));
		for (AActor* Actor : Actors)
		{
			Actor->Modify();
			if (bKeepOffset)
			{
				Actor->SetActorLocation(
					Actor->GetActorLocation() + Delta,
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
			}
			else
			{
				const FQuat CursorRotation = CursorTransform.GetRotation();
				const FVector PivotOffset = Actor->GetPivotOffset() * Actor->GetActorScale3D();
				const FVector NewLocation = CursorTransform.GetLocation() - CursorRotation.RotateVector(PivotOffset);
				Actor->SetActorRotation(CursorRotation, ETeleportType::TeleportPhysics);
				Actor->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
			}
			Actor->PostEditMove(true);
			Actor->MarkPackageDirty();
		}

		if (GEditor)
		{
			GEditor->NoteSelectionChange();
			GEditor->RedrawLevelEditingViewports(true);
		}
		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("%s moved %d actor(s) to 3D cursor."),
			bKeepOffset ? TEXT("SelectionToCursorOffset") : TEXT("SelectionToCursor"),
			Actors.Num());
		return true;
	}

	bool SetCursorToSelected(FBlendViewSceneCursor& SceneCursor)
	{
		TArray<AActor*> Actors;
		GetSelectedEditableActors(Actors);
		if (Actors.IsEmpty())
		{
			UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("CursorToSelected failed: no editable selected actors."));
			return false;
		}

		if (Actors.Num() == 1)
		{
			UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("CursorToSelected using single actor pivot: %s"), *Actors[0]->GetName());
			SetCursorTransform(
				SceneCursor,
				FTransform(
					Actors[0]->GetActorQuat(),
					GetActorPivotWorldLocation(*Actors[0]),
					FVector::OneVector));
			return true;
		}

		FBox SelectionBounds;
		if (!GetSelectionBounds(Actors, SelectionBounds))
		{
			UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("CursorToSelected failed: invalid selection bounds."));
			return false;
		}

		UE_LOG(LogBlendViewPieMenu, Verbose, TEXT("CursorToSelected using bounds center for %d actors."), Actors.Num());
		SetCursorTransform(
			SceneCursor,
			FTransform(
				ResolveSelectionCursorRotation(Actors),
				SelectionBounds.GetCenter(),
				FVector::OneVector));
		return true;
	}

	bool SetOriginsToGeometry()
	{
		TArray<AActor*> Actors;
		GetSelectedEditableActors(Actors);
		if (Actors.IsEmpty())
		{
			return false;
		}

		const FScopedTransaction Transaction(
			NSLOCTEXT("BlendView", "MoveOriginsToGeometry", "BlendView Move Origins to Geometry"));
		for (AActor* Actor : Actors)
		{
			const FBox Bounds = GetActorWorldBounds(*Actor);
			if (Bounds.IsValid)
			{
				SetActorPivotWorldLocation(*Actor, Bounds.GetCenter());
			}
		}

		if (GEditor)
		{
			RefreshOriginChanges();
		}
		return true;
	}

	bool SetOriginsToCursor(FBlendViewSceneCursor& SceneCursor)
	{
		FTransform CursorTransform;
		if (!TryGetCursorTransform(SceneCursor, CursorTransform))
		{
			return false;
		}

		TArray<AActor*> Actors;
		GetSelectedEditableActors(Actors);
		if (Actors.IsEmpty())
		{
			return false;
		}

		const FScopedTransaction Transaction(
			NSLOCTEXT("BlendView", "MoveOriginsToCursor", "BlendView Move Origins to Cursor"));
		for (AActor* Actor : Actors)
		{
			SetActorPivotWorldLocation(*Actor, CursorTransform.GetLocation());
		}

		if (GEditor)
		{
			RefreshOriginChanges();
		}
		return true;
	}

	bool SetOriginsToActive()
	{
		AActor* ActiveActor = FBlendViewSessionState::GetActiveActor();
		if (!IsValid(ActiveActor))
		{
			return false;
		}

		TArray<AActor*> Actors;
		GetSelectedEditableActors(Actors);
		if (Actors.Num() <= 1 || !Actors.Contains(ActiveActor))
		{
			return false;
		}

		const FVector ActivePivotLocation = GetActorPivotWorldLocation(*ActiveActor);
		const FScopedTransaction Transaction(
			NSLOCTEXT("BlendView", "MoveOriginsToActive", "BlendView Move Origins to Active"));
		for (AActor* Actor : Actors)
		{
			if (Actor != ActiveActor)
			{
				SetActorPivotWorldLocation(*Actor, ActivePivotLocation);
			}
		}

		if (GEditor)
		{
			RefreshOriginChanges();
		}
		return true;
	}

	bool SetOriginsToBottom()
	{
		TArray<AActor*> Actors;
		GetSelectedEditableActors(Actors);
		if (Actors.IsEmpty())
		{
			return false;
		}

		const FScopedTransaction Transaction(
			NSLOCTEXT("BlendView", "MoveOriginsToBottom", "BlendView Move Origins to Bottom"));
		for (AActor* Actor : Actors)
		{
			const FBox Bounds = GetActorWorldBounds(*Actor);
			if (Bounds.IsValid)
			{
				SetActorPivotWorldLocation(
					*Actor,
					FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z));
			}
		}

		if (GEditor)
		{
			RefreshOriginChanges();
		}
		return true;
	}
}

bool FBlendViewCursorOriginActions::Execute(
	const EBlendViewCursorOriginAction Action,
	FBlendViewSceneCursor& SceneCursor)
{
	switch (Action)
	{
	case EBlendViewCursorOriginAction::CursorToOrigin:
		SetCursorTransform(SceneCursor, FTransform::Identity);
		return true;
	case EBlendViewCursorOriginAction::CursorToSelected:
		return SetCursorToSelected(SceneCursor);
	case EBlendViewCursorOriginAction::SelectionToCursorOffset:
		return MoveSelectionToCursor(true, SceneCursor);
	case EBlendViewCursorOriginAction::SelectionToCursor:
		return MoveSelectionToCursor(false, SceneCursor);
	case EBlendViewCursorOriginAction::OriginToGeometry:
		return SetOriginsToGeometry();
	case EBlendViewCursorOriginAction::OriginToCursor:
		return SetOriginsToCursor(SceneCursor);
	case EBlendViewCursorOriginAction::OriginToActive:
		return SetOriginsToActive();
	case EBlendViewCursorOriginAction::OriginToBottom:
		return SetOriginsToBottom();
	default:
		return false;
	}
}

bool FBlendViewCursorOriginActions::CanExecute(const EBlendViewCursorOriginAction Action)
{
	if (Action != EBlendViewCursorOriginAction::OriginToActive)
	{
		return true;
	}

	TArray<AActor*> Actors;
	GetSelectedEditableActors(Actors);
	const AActor* ActiveActor = FBlendViewSessionState::GetActiveActor();
	return Actors.Num() > 1 && IsValid(ActiveActor) && Actors.Contains(ActiveActor);
}

const TCHAR* FBlendViewCursorOriginActions::ToLogName(const EBlendViewCursorOriginAction Action)
{
	switch (Action)
	{
	case EBlendViewCursorOriginAction::CursorToOrigin:
		return TEXT("CursorToOrigin");
	case EBlendViewCursorOriginAction::CursorToSelected:
		return TEXT("CursorToSelected");
	case EBlendViewCursorOriginAction::SelectionToCursorOffset:
		return TEXT("SelectionToCursorOffset");
	case EBlendViewCursorOriginAction::SelectionToCursor:
		return TEXT("SelectionToCursor");
	case EBlendViewCursorOriginAction::OriginToGeometry:
		return TEXT("OriginToGeometry");
	case EBlendViewCursorOriginAction::OriginToCursor:
		return TEXT("OriginToCursor");
	case EBlendViewCursorOriginAction::OriginToActive:
		return TEXT("OriginToActive");
	case EBlendViewCursorOriginAction::OriginToBottom:
		return TEXT("OriginToBottom");
	default:
		return TEXT("Unknown");
	}
}
