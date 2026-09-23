// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Selection/BlendViewActiveSelectionTracker.h"

#include "Components/ActorComponent.h"
#include "Core/BlendViewSessionState.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Selection.h"

namespace
{
	AActor* GetActorFromSelectionObject(UObject* Object)
	{
		if (AActor* Actor = Cast<AActor>(Object))
		{
			return Actor;
		}
		if (const UActorComponent* Component = Cast<UActorComponent>(Object))
		{
			return Component->GetOwner();
		}
		return nullptr;
	}
}

void FBlendViewActiveSelectionTracker::Startup()
{
	if (bStarted)
	{
		return;
	}

	bStarted = true;
	SelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw(
		this,
		&FBlendViewActiveSelectionTracker::OnSelectionChanged);
	SelectObjectHandle = USelection::SelectObjectEvent.AddRaw(
		this,
		&FBlendViewActiveSelectionTracker::OnSelectObject);
	ScheduleRefresh();
}

void FBlendViewActiveSelectionTracker::Shutdown()
{
	if (!bStarted)
	{
		return;
	}

	if (SelectionChangedHandle.IsValid())
	{
		USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
		SelectionChangedHandle.Reset();
	}
	if (SelectObjectHandle.IsValid())
	{
		USelection::SelectObjectEvent.Remove(SelectObjectHandle);
		SelectObjectHandle.Reset();
	}
	if (RefreshTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RefreshTickerHandle);
		RefreshTickerHandle.Reset();
	}

	PendingActiveActor.Reset();
	ActiveActor.Reset();
	FBlendViewSessionState::ClearActiveActor();
	bRefreshPending = false;
	bStarted = false;
}

void FBlendViewActiveSelectionTracker::OnSelectionChanged(UObject* Selection)
{
	ScheduleRefresh();
}

void FBlendViewActiveSelectionTracker::OnSelectObject(UObject* Object)
{
	if (AActor* Actor = GetActorFromSelectionObject(Object); IsValid(Actor))
	{
		PendingActiveActor = Actor;
	}
	ScheduleRefresh();
}

void FBlendViewActiveSelectionTracker::ScheduleRefresh()
{
	if (!bStarted || bRefreshPending)
	{
		return;
	}

	bRefreshPending = true;
	RefreshTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FBlendViewActiveSelectionTracker::HandleDeferredRefresh),
		0.0f);
}

bool FBlendViewActiveSelectionTracker::HandleDeferredRefresh(float DeltaTime)
{
	RefreshTickerHandle.Reset();
	bRefreshPending = false;
	Refresh();
	return false;
}

void FBlendViewActiveSelectionTracker::Refresh()
{
	if (!bStarted || !GEditor)
	{
		return;
	}

	USelection* SelectedActorsSelection = GEditor->GetSelectedActors();
	if (!SelectedActorsSelection)
	{
		PendingActiveActor.Reset();
		ActiveActor.Reset();
		FBlendViewSessionState::ClearActiveActor();
		return;
	}

	TArray<AActor*> SelectedActors;
	SelectedActorsSelection->GetSelectedObjects<AActor>(SelectedActors);
	if (SelectedActors.IsEmpty())
	{
		PendingActiveActor.Reset();
		ActiveActor.Reset();
		FBlendViewSessionState::ClearActiveActor();
		return;
	}

	ActiveActor = ResolveActiveActor(SelectedActors);
	PendingActiveActor.Reset();
	FBlendViewSessionState::SetActiveActor(ActiveActor.Get());
}

AActor* FBlendViewActiveSelectionTracker::ResolveActiveActor(const TArray<AActor*>& SelectedActors) const
{
	if (AActor* PendingActor = PendingActiveActor.Get();
		IsValid(PendingActor) && SelectedActors.Contains(PendingActor))
	{
		return PendingActor;
	}

	if (AActor* CurrentActiveActor = ActiveActor.Get();
		IsValid(CurrentActiveActor) && SelectedActors.Contains(CurrentActiveActor))
	{
		return CurrentActiveActor;
	}

	for (int32 Index = SelectedActors.Num() - 1; Index >= 0; --Index)
	{
		if (IsValid(SelectedActors[Index]))
		{
			return SelectedActors[Index];
		}
	}
	return nullptr;
}
