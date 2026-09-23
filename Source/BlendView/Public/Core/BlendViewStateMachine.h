// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EBlendViewInteractionState : uint8
{
	Idle,
	NavigationActive,
	ModalToolActive
};

class FBlendViewStateMachine
{
public:
	EBlendViewInteractionState GetState() const { return State; }

	void EnterNavigation();
	void EnterModalTool();
	void Reset();

private:
	EBlendViewInteractionState State = EBlendViewInteractionState::Idle;
};
