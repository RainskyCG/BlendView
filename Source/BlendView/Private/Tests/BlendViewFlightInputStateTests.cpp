// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Core/BlendViewFlightInputState.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FBlendViewInputEvent MakeFlightInputEvent(
		const EBlendViewInputEventType Type,
		const FKey& Key,
		const bool bShiftDown = false,
		const bool bAltDown = false)
	{
		FBlendViewInputEvent Event;
		Event.Type = Type;
		Event.Key = Key;
		Event.bShiftDown = bShiftDown;
		Event.bAltDown = bAltDown;
		return Event;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewFlightInputOrderTest,
	"BlendView.Input.FlightBoost.KeyOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewFlightInputOrderTest::RunTest(const FString& Parameters)
{
	FBlendViewFlightInputState State;

	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseDown, EKeys::RightMouseButton));
	TestFalse(TEXT("RMB alone does not modify speed"), State.ShouldModifySpeed());
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::LeftShift, true));
	TestTrue(TEXT("RMB then Shift enables speed modification"), State.ShouldModifySpeed());
	TestEqual(TEXT("Shift multiplies flight speed"), State.GetSpeedMultiplier(5.0f), 5.0f);

	State.Reset();
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::RightShift, true));
	TestFalse(TEXT("Shift alone does not modify speed"), State.ShouldModifySpeed());
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseDown, EKeys::RightMouseButton, true));
	TestTrue(TEXT("Shift then RMB enables speed modification"), State.ShouldModifySpeed());

	State.Reset();
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseDown, EKeys::RightMouseButton));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::LeftAlt, false, true));
	TestTrue(TEXT("RMB then Alt enables speed modification"), State.ShouldModifySpeed());
	TestEqual(TEXT("Alt divides flight speed"), State.GetSpeedMultiplier(5.0f), 0.2f);

	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::LeftShift, true, true));
	TestFalse(TEXT("Shift and Alt together cancel speed modification"), State.ShouldModifySpeed());
	TestEqual(TEXT("Shift and Alt together keep original speed"), State.GetSpeedMultiplier(5.0f), 1.0f);

	State.Reset();
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseDown, EKeys::RightMouseButton));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::E, false, true));
	TestTrue(TEXT("Navigation key modifier state can enter slow mode"), State.ShouldModifySpeed());
	TestEqual(TEXT("Navigation key Alt modifier divides flight speed"), State.GetSpeedMultiplier(5.0f), 0.2f);
	TestTrue(TEXT("Navigation key state is tracked while held"), State.HasNavigationInput());
	TestEqual(TEXT("E contributes world-up flight impulse"), State.GetWorldUpDownImpulse(), 1.0f);
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyUp, EKeys::E, false, true));
	TestFalse(TEXT("Navigation key state clears on release"), State.HasNavigationInput());

	State.Reset();
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::LeftAlt, false, true));
	TestFalse(TEXT("Alt alone does not modify speed"), State.ShouldModifySpeed());
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseDown, EKeys::RightMouseButton, false, true));
	TestTrue(TEXT("Alt before RMB enables slow mode once RMB is held"), State.ShouldModifySpeed());
	TestEqual(TEXT("Pre-held Alt divides flight speed"), State.GetSpeedMultiplier(5.0f), 0.2f);

	for (const FKey& Key : {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::Q, EKeys::E})
	{
		TestTrue(
			*FString::Printf(TEXT("%s is isolated during the flight session"), *Key.ToString()),
			FBlendViewFlightInputState::IsNavigationKey(Key));
	}
	TestTrue(TEXT("Left Alt is recognized as a flight speed modifier"), FBlendViewFlightInputState::IsAltKey(EKeys::LeftAlt));
	TestTrue(TEXT("Right Alt is recognized as a flight speed modifier"), FBlendViewFlightInputState::IsAltKey(EKeys::RightAlt));
	TestFalse(
		TEXT("Unrelated editor shortcuts remain outside the flight session"),
		FBlendViewFlightInputState::IsNavigationKey(EKeys::G));

	State.Reset();
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::W));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::A));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::Q));
	TestEqual(TEXT("W contributes forward flight impulse"), State.GetForwardBackwardImpulse(), 1.0f);
	TestEqual(TEXT("A contributes left flight impulse"), State.GetRightLeftImpulse(), -1.0f);
	TestEqual(TEXT("Q contributes world-down flight impulse"), State.GetWorldUpDownImpulse(), -1.0f);
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::S));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::D));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::E));
	TestEqual(TEXT("Opposing forward keys cancel"), State.GetForwardBackwardImpulse(), 0.0f);
	TestEqual(TEXT("Opposing lateral keys cancel"), State.GetRightLeftImpulse(), 0.0f);
	TestEqual(TEXT("Opposing vertical keys cancel"), State.GetWorldUpDownImpulse(), 0.0f);

	State.ClearNavigationInput();
	TestFalse(TEXT("Explicit navigation clear removes held movement"), State.HasNavigationInput());
	State.SetNavigationKeyState(EKeys::D, true);
	TestEqual(TEXT("Explicit sampled navigation state contributes impulse"), State.GetRightLeftImpulse(), 1.0f);
	TArray<FKey> PressedNavigationKeys;
	State.AppendPressedNavigationKeys(PressedNavigationKeys);
	TestEqual(TEXT("Pressed navigation keys can be sampled"), PressedNavigationKeys.Num(), 1);
	TestEqual(TEXT("Sampled navigation key matches held input"), PressedNavigationKeys[0], EKeys::D);
	State.SetNavigationKeyState(EKeys::D, false);
	TestFalse(TEXT("Explicit sampled navigation release clears movement"), State.HasNavigationInput());

	State.Reset();
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseDown, EKeys::RightMouseButton));
	State.SetModifierState(true, false);
	TestTrue(TEXT("Sampled Shift enables speed modification inside RMB flight"), State.ShouldModifySpeed());
	State.SetModifierState(false, true);
	TestTrue(TEXT("Sampled Alt enables speed modification inside RMB flight"), State.ShouldModifySpeed());
	TestEqual(TEXT("Sampled Alt divides flight speed"), State.GetSpeedMultiplier(5.0f), 0.2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewFlightInputReleaseTest,
	"BlendView.Input.FlightBoost.ReleaseAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewFlightInputReleaseTest::RunTest(const FString& Parameters)
{
	FBlendViewFlightInputState State;
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseDown, EKeys::RightMouseButton, true));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::LeftShift, true));
	TestTrue(TEXT("Precondition: speed modification is active"), State.ShouldModifySpeed());

	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyUp, EKeys::LeftShift, false));
	TestFalse(TEXT("Releasing the last Shift disables speed modification"), State.ShouldModifySpeed());
	TestTrue(TEXT("RMB remains active after Shift release"), State.IsRightMouseDown());

	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::RightShift, true));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseUp, EKeys::RightMouseButton, true));
	TestFalse(TEXT("Releasing RMB disables speed modification"), State.ShouldModifySpeed());
	TestTrue(TEXT("Shift state remains accurate after RMB release"), State.IsShiftDown());

	State.Reset();
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::MouseDown, EKeys::RightMouseButton, false, true));
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyDown, EKeys::LeftAlt, false, true));
	TestTrue(TEXT("Precondition: slow mode is active"), State.ShouldModifySpeed());
	State.Update(MakeFlightInputEvent(EBlendViewInputEventType::KeyUp, EKeys::LeftAlt, false, false));
	TestFalse(TEXT("Releasing the last Alt disables speed modification"), State.ShouldModifySpeed());
	TestTrue(TEXT("RMB remains active after Alt release"), State.IsRightMouseDown());

	State.Reset();
	TestFalse(TEXT("Reset clears RMB"), State.IsRightMouseDown());
	TestFalse(TEXT("Reset clears Shift"), State.IsShiftDown());
	TestFalse(TEXT("Reset clears Alt"), State.IsAltDown());
	return true;
}

#endif
