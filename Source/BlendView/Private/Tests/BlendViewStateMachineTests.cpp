// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Core/BlendViewStateMachine.h"
#include "Misc/AutomationTest.h"
#include "Tools/BlendViewPivotEditSession.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewPivotEditSessionTest,
	"BlendView.StateMachine.PivotEditSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewPivotEditSessionTest::RunTest(const FString& Parameters)
{
	FBlendViewPivotEditSession Session;
	TestFalse(TEXT("Initial session has no pivot mode"), Session.IsAnyPivotMode());

	Session.EnterActorPivotMode();
	TestTrue(TEXT("Actor pivot mode is active"), Session.IsActorPivotMode());
	TestFalse(TEXT("Modeling pivot mode is inactive"), Session.IsModelingPivotMode());

	Session.EnterModelingPivotMode();
	TestFalse(TEXT("Actor pivot mode exits when modeling pivot starts"), Session.IsActorPivotMode());
	TestTrue(TEXT("Modeling pivot mode is active"), Session.IsModelingPivotMode());

	Session.RequestModelingPivotInputReanchor();
	TestTrue(TEXT("Pending reanchor is consumed once"), Session.ConsumeModelingPivotInputReanchor());
	TestFalse(TEXT("Pending reanchor is cleared after consume"), Session.ConsumeModelingPivotInputReanchor());

	Session.Reset();
	TestFalse(TEXT("Reset clears all pivot modes"), Session.IsAnyPivotMode());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewStateMachineLifecycleTest,
	"BlendView.StateMachine.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewStateMachineLifecycleTest::RunTest(const FString& Parameters)
{
	FBlendViewStateMachine StateMachine;
	TestEqual(TEXT("Initial state is idle"), StateMachine.GetState(), EBlendViewInteractionState::Idle);

	StateMachine.EnterNavigation();
	TestEqual(TEXT("Navigation transition"), StateMachine.GetState(), EBlendViewInteractionState::NavigationActive);

	StateMachine.EnterModalTool();
	TestEqual(TEXT("Modal tool transition"), StateMachine.GetState(), EBlendViewInteractionState::ModalToolActive);

	StateMachine.Reset();
	TestEqual(TEXT("Reset returns to idle"), StateMachine.GetState(), EBlendViewInteractionState::Idle);
	return true;
}

#endif
