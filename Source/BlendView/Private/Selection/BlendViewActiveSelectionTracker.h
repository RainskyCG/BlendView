// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class AActor;
class UObject;

class FBlendViewActiveSelectionTracker final
{
public:
	void Startup();
	void Shutdown();

private:
	void OnSelectionChanged(UObject* Selection);
	void OnSelectObject(UObject* Object);
	void ScheduleRefresh();
	bool HandleDeferredRefresh(float DeltaTime);
	void Refresh();
	AActor* ResolveActiveActor(const TArray<AActor*>& SelectedActors) const;

	FDelegateHandle SelectionChangedHandle;
	FDelegateHandle SelectObjectHandle;
	FTSTicker::FDelegateHandle RefreshTickerHandle;
	TWeakObjectPtr<AActor> PendingActiveActor;
	TWeakObjectPtr<AActor> ActiveActor;
	bool bRefreshPending = false;
	bool bStarted = false;
};
