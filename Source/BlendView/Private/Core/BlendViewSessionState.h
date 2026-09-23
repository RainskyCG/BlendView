// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

class FBlendViewSessionState
{
public:
	static bool AreStatusHintsVisible();
	static void ToggleStatusHints();
	static bool IsPersistentSnapEnabled();
	static void TogglePersistentSnap();
	static void SetPersistentSnapEnabled(bool bEnabled);
	static void SetActiveActor(AActor* Actor);
	static AActor* GetActiveActor();
	static void ClearActiveActor();
	static void SetSceneCursorTransform(const FTransform& Transform);
	static bool TryGetSceneCursorTransform(FTransform& OutTransform);

private:
	static TWeakObjectPtr<AActor> ActiveActor;
	static FTransform SceneCursorTransform;
	static bool bStatusHintsVisible;
	static bool bPersistentSnapEnabled;
	static bool bHasSceneCursorTransform;
};
