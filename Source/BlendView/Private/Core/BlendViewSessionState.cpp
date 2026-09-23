// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Core/BlendViewSessionState.h"

TWeakObjectPtr<AActor> FBlendViewSessionState::ActiveActor;
FTransform FBlendViewSessionState::SceneCursorTransform = FTransform::Identity;
bool FBlendViewSessionState::bStatusHintsVisible = true;
bool FBlendViewSessionState::bPersistentSnapEnabled = false;
bool FBlendViewSessionState::bHasSceneCursorTransform = false;

bool FBlendViewSessionState::AreStatusHintsVisible()
{
	return bStatusHintsVisible;
}

void FBlendViewSessionState::ToggleStatusHints()
{
	bStatusHintsVisible = !bStatusHintsVisible;
}

bool FBlendViewSessionState::IsPersistentSnapEnabled()
{
	return bPersistentSnapEnabled;
}

void FBlendViewSessionState::TogglePersistentSnap()
{
	bPersistentSnapEnabled = !bPersistentSnapEnabled;
}

void FBlendViewSessionState::SetPersistentSnapEnabled(const bool bEnabled)
{
	bPersistentSnapEnabled = bEnabled;
}

void FBlendViewSessionState::SetActiveActor(AActor* Actor)
{
	ActiveActor = Actor;
}

AActor* FBlendViewSessionState::GetActiveActor()
{
	return ActiveActor.Get();
}

void FBlendViewSessionState::ClearActiveActor()
{
	ActiveActor.Reset();
}

void FBlendViewSessionState::SetSceneCursorTransform(const FTransform& Transform)
{
	SceneCursorTransform = Transform;
	bHasSceneCursorTransform = true;
}

bool FBlendViewSessionState::TryGetSceneCursorTransform(FTransform& OutTransform)
{
	if (!bHasSceneCursorTransform)
	{
		return false;
	}

	OutTransform = SceneCursorTransform;
	return true;
}
