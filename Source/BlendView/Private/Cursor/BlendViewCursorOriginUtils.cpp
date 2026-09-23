// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Cursor/BlendViewCursorOriginUtils.h"

#include "Core/BlendViewSessionState.h"
#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "Editor/UnrealEdEngine.h"
#include "GameFramework/Actor.h"
#include "Selection.h"
#include "UnrealEdGlobals.h"

namespace BlendViewCursorOriginUtils
{
	void GetSelectedEditableActors(TArray<AActor*>& OutActors)
	{
		OutActors.Reset();
		if (!GEditor)
		{
			return;
		}

		USelection* SelectedActors = GEditor->GetSelectedActors();
		if (!SelectedActors)
		{
			return;
		}

		SelectedActors->GetSelectedObjects<AActor>(OutActors);
		OutActors.RemoveAll([](const AActor* Actor)
		{
			return !IsValid(Actor) || Actor->IsTemplate();
		});
	}

	FBox GetActorWorldBounds(const AActor& Actor)
	{
		FBox Bounds = Actor.GetComponentsBoundingBox(true);
		if (!Bounds.IsValid)
		{
			Bounds += Actor.GetActorLocation();
		}
		return Bounds;
	}

	bool GetSelectionBounds(const TArray<AActor*>& Actors, FBox& OutBounds)
	{
		OutBounds.Init();
		for (const AActor* Actor : Actors)
		{
			if (!IsValid(Actor))
			{
				continue;
			}

			const FBox ActorBounds = GetActorWorldBounds(*Actor);
			if (ActorBounds.IsValid)
			{
				OutBounds += ActorBounds;
			}
		}
		return OutBounds.IsValid != 0;
	}

	FVector GetActorPivotWorldLocation(const AActor& Actor)
	{
		return Actor.GetTransform().TransformPosition(Actor.GetPivotOffset());
	}

	FQuat ResolveSelectionCursorRotation(const TArray<AActor*>& Actors)
	{
		if (AActor* ActiveActor = FBlendViewSessionState::GetActiveActor();
			IsValid(ActiveActor) && Actors.Contains(ActiveActor))
		{
			return ActiveActor->GetActorQuat();
		}

		for (const AActor* Actor : Actors)
		{
			if (IsValid(Actor))
			{
				return Actor->GetActorQuat();
			}
		}

		return FQuat::Identity;
	}

	void SetActorPivotWorldLocation(AActor& Actor, const FVector& WorldLocation)
	{
		Actor.Modify();
		Actor.SetPivotOffset(Actor.GetTransform().InverseTransformPosition(WorldLocation));
		Actor.PostEditMove(true);
		Actor.MarkPackageDirty();
	}

	void RefreshOriginChanges()
	{
		if (GUnrealEd)
		{
			GUnrealEd->UpdatePivotLocationForSelection(true);
			GUnrealEd->NoteSelectionChange();
		}
		else if (GEditor)
		{
			GEditor->NoteSelectionChange();
		}

		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}

		FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
		FEditorSupportDelegates::UpdateUI.Broadcast();
	}
}
