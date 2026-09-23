// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewInputTypes.h"

struct FBlendViewViewportContext;
class UCanvas;
enum class EBlendViewTransformResetChannel : uint8;

class IBlendViewModalTool
{
public:
	virtual ~IBlendViewModalTool() = default;

	virtual bool Begin(const FBlendViewViewportContext& Context) = 0;
	virtual bool WantsInputBeforeModalShortcuts(const FBlendViewInputEvent& Event) const { return false; }
	virtual EBlendViewInputResult HandleInput(const FBlendViewInputEvent& Event) = 0;
	virtual bool ResetCurrentTransform(EBlendViewTransformResetChannel Channel) { return false; }
	virtual void Tick(float DeltaTime) {}
	virtual void DrawHUD(UCanvas* Canvas) {}
	virtual bool IsComplete() const { return false; }
	virtual void Confirm() = 0;
	virtual void Cancel() = 0;
	virtual void End() = 0;
	virtual void PrepareForEngineExit() {}
};
