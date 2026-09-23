// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Core/BlendViewToolManager.h"

#include "Core/BlendViewModalTool.h"

bool FBlendViewToolManager::BeginTool(TUniquePtr<IBlendViewModalTool> Tool, const FBlendViewViewportContext& Context)
{
	if (!Tool.IsValid() || ActiveTool.IsValid())
	{
		return false;
	}

	if (!Tool->Begin(Context))
	{
		return false;
	}

	ActiveTool = MoveTemp(Tool);
	return true;
}

bool FBlendViewToolManager::WantsInputBeforeModalShortcuts(const FBlendViewInputEvent& Event) const
{
	return ActiveTool.IsValid() && ActiveTool->WantsInputBeforeModalShortcuts(Event);
}

EBlendViewInputResult FBlendViewToolManager::RouteInput(const FBlendViewInputEvent& Event)
{
	return ActiveTool.IsValid() ? ActiveTool->HandleInput(Event) : EBlendViewInputResult::PassThrough;
}

bool FBlendViewToolManager::TryResetCurrentTransform(const EBlendViewTransformResetChannel Channel)
{
	return ActiveTool.IsValid() && ActiveTool->ResetCurrentTransform(Channel);
}

bool FBlendViewToolManager::Tick(float DeltaTime)
{
	if (ActiveTool.IsValid())
	{
		ActiveTool->Tick(DeltaTime);
		if (ActiveTool.IsValid() && ActiveTool->IsComplete())
		{
			EndActiveTool();
			return true;
		}
	}
	return false;
}

void FBlendViewToolManager::DrawHUD(UCanvas* Canvas)
{
	if (ActiveTool.IsValid())
	{
		ActiveTool->DrawHUD(Canvas);
	}
}

void FBlendViewToolManager::ConfirmActiveTool()
{
	if (ActiveTool.IsValid())
	{
		ActiveTool->Confirm();
		EndActiveTool();
	}
}

void FBlendViewToolManager::CancelActiveTool()
{
	if (ActiveTool.IsValid())
	{
		ActiveTool->Cancel();
		EndActiveTool();
	}
}

void FBlendViewToolManager::EndActiveTool()
{
	if (ActiveTool.IsValid())
	{
		ActiveTool->End();
		ActiveTool.Reset();
	}
}

void FBlendViewToolManager::PrepareForEngineExit()
{
	if (ActiveTool.IsValid())
	{
		ActiveTool->PrepareForEngineExit();
		ActiveTool.Reset();
	}
}
