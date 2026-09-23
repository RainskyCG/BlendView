// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FEditorViewportClient;
class FSceneViewport;
class FViewport;
class SWidget;

enum class EBlendViewViewportKind : uint8
{
	Unknown,
	EditorViewport,
	LevelEditor
};

struct FBlendViewViewportContext
{
	FEditorViewportClient* ViewportClient = nullptr;
	FViewport* Viewport = nullptr;
	FSceneViewport* SceneViewport = nullptr;
	TWeakPtr<SWidget> ViewportWidget;
	EBlendViewViewportKind Kind = EBlendViewViewportKind::Unknown;
	FVector2D MouseScreenPosition = FVector2D::ZeroVector;
	FVector2D MouseViewportPosition = FVector2D::ZeroVector;
	FIntPoint ViewportSize = FIntPoint::ZeroValue;

	bool IsValid() const
	{
		return ViewportClient != nullptr && Viewport != nullptr;
	}
};

class FBlendViewViewportResolver
{
public:
	FBlendViewViewportContext ResolveForMousePosition(const FVector2D& ScreenPosition) const;
};
