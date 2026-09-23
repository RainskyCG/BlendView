// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewInputTypes.h"
#include "Core/BlendViewModalTool.h"

class UCanvas;
struct FBlendViewViewportContext;
enum class EBlendViewTransformResetChannel : uint8;

class FBlendViewToolManager
{
public:
	bool HasActiveTool() const { return ActiveTool.IsValid(); }

	bool BeginTool(TUniquePtr<IBlendViewModalTool> Tool, const FBlendViewViewportContext& Context);
	bool WantsInputBeforeModalShortcuts(const FBlendViewInputEvent& Event) const;
	EBlendViewInputResult RouteInput(const FBlendViewInputEvent& Event);
	bool TryResetCurrentTransform(EBlendViewTransformResetChannel Channel);
	bool Tick(float DeltaTime);
	void DrawHUD(UCanvas* Canvas);
	void ConfirmActiveTool();
	void CancelActiveTool();
	void PrepareForEngineExit();

private:
	void EndActiveTool();

	TUniquePtr<IBlendViewModalTool> ActiveTool;
};
