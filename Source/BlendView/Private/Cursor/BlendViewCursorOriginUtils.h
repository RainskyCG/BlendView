// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

namespace BlendViewCursorOriginUtils
{
	void GetSelectedEditableActors(TArray<AActor*>& OutActors);
	FBox GetActorWorldBounds(const AActor& Actor);
	bool GetSelectionBounds(const TArray<AActor*>& Actors, FBox& OutBounds);
	FVector GetActorPivotWorldLocation(const AActor& Actor);
	FQuat ResolveSelectionCursorRotation(const TArray<AActor*>& Actors);
	void SetActorPivotWorldLocation(AActor& Actor, const FVector& WorldLocation);
	void RefreshOriginChanges();
}
