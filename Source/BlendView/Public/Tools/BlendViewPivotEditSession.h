// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EBlendViewPivotEditSessionMode : uint8
{
	None,
	ActorPivot,
	ModelingPivot
};

class FBlendViewPivotEditSession final
{
public:
	bool IsActorPivotMode() const
	{
		return Mode == EBlendViewPivotEditSessionMode::ActorPivot;
	}

	bool IsModelingPivotMode() const
	{
		return Mode == EBlendViewPivotEditSessionMode::ModelingPivot;
	}

	bool IsAnyPivotMode() const
	{
		return Mode != EBlendViewPivotEditSessionMode::None;
	}

	void EnterActorPivotMode()
	{
		Mode = EBlendViewPivotEditSessionMode::ActorPivot;
		bPendingModelingPivotInputReanchor = false;
	}

	void EnterModelingPivotMode()
	{
		Mode = EBlendViewPivotEditSessionMode::ModelingPivot;
	}

	void ExitActorPivotMode()
	{
		if (IsActorPivotMode())
		{
			Mode = EBlendViewPivotEditSessionMode::None;
		}
	}

	void Reset()
	{
		Mode = EBlendViewPivotEditSessionMode::None;
		bPendingModelingPivotInputReanchor = false;
	}

	void RequestModelingPivotInputReanchor()
	{
		bPendingModelingPivotInputReanchor = true;
	}

	bool ConsumeModelingPivotInputReanchor()
	{
		const bool bConsume = bPendingModelingPivotInputReanchor;
		bPendingModelingPivotInputReanchor = false;
		return bConsume;
	}

private:
	EBlendViewPivotEditSessionMode Mode = EBlendViewPivotEditSessionMode::None;
	bool bPendingModelingPivotInputReanchor = false;
};
