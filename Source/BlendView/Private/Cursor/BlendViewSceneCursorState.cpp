// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Cursor/BlendViewSceneCursorState.h"

#include "Core/BlendViewSessionState.h"
#include "Cursor/BlendViewSceneCursor.h"

bool FBlendViewSceneCursorState::TryGetTransform(
	const FBlendViewSceneCursor& SceneCursor,
	FTransform& OutTransform)
{
	if (SceneCursor.IsVisible())
	{
		OutTransform = SceneCursor.GetTransform();
		return true;
	}
	return TryGetSavedTransform(OutTransform);
}

bool FBlendViewSceneCursorState::TryGetSavedTransform(FTransform& OutTransform)
{
	return FBlendViewSessionState::TryGetSceneCursorTransform(OutTransform);
}

void FBlendViewSceneCursorState::SetTransform(
	FBlendViewSceneCursor& SceneCursor,
	const FTransform& Transform)
{
	SceneCursor.SetTransform(Transform);
	FBlendViewSessionState::SetSceneCursorTransform(SceneCursor.GetTransform());
}

void FBlendViewSceneCursorState::SetLocationPreservingRotation(
	FBlendViewSceneCursor& SceneCursor,
	const FVector& Location)
{
	SceneCursor.SetLocationPreservingRotation(Location);
	FBlendViewSessionState::SetSceneCursorTransform(SceneCursor.GetTransform());
}
