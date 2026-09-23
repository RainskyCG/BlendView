// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Core/BlendViewStateMachine.h"

void FBlendViewStateMachine::EnterNavigation()
{
	State = EBlendViewInteractionState::NavigationActive;
}

void FBlendViewStateMachine::EnterModalTool()
{
	State = EBlendViewInteractionState::ModalToolActive;
}

void FBlendViewStateMachine::Reset()
{
	State = EBlendViewInteractionState::Idle;
}
