// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Tools/BlendViewNumericInput.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewNumericInputEditingTest,
	"BlendView.Transform.NumericInput.Editing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewNumericInputEditingTest::RunTest(const FString& Parameters)
{
	FBlendViewNumericInput Input;
	TestFalse(TEXT("Unrelated keys pass through"), Input.HandleKey(EKeys::A));
	TestFalse(TEXT("Input remains inactive"), Input.IsActive());

	Input.HandleKey(EKeys::Hyphen);
	Input.HandleKey(EKeys::Period);
	Input.HandleKey(EKeys::Five);
	TestEqual(TEXT("Leading decimal is normalized"), Input.GetBuffer(), FString(TEXT("-0.5")));
	TestEqual(TEXT("Buffer parses to a signed value"), Input.GetValue(), -0.5);

	Input.HandleKey(EKeys::Decimal);
	TestEqual(TEXT("A second decimal point is ignored"), Input.GetBuffer(), FString(TEXT("-0.5")));

	Input.HandleKey(EKeys::Subtract);
	TestEqual(TEXT("Minus toggles the sign"), Input.GetBuffer(), FString(TEXT("0.5")));
	TestEqual(TEXT("Toggled value is positive"), Input.GetValue(), 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewNumericInputBackspaceTest,
	"BlendView.Transform.NumericInput.Backspace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewNumericInputBackspaceTest::RunTest(const FString& Parameters)
{
	FBlendViewNumericInput Input;
	TestEqual(
		TEXT("Backspace is ignored while inactive"),
		Input.HandleBackspace(),
		EBlendViewNumericBackspaceResult::Ignored);

	Input.HandleKey(EKeys::One);
	Input.HandleKey(EKeys::Two);
	TestEqual(
		TEXT("Backspace updates a non-empty buffer"),
		Input.HandleBackspace(),
		EBlendViewNumericBackspaceResult::Updated);
	TestEqual(TEXT("Last digit is removed"), Input.GetBuffer(), FString(TEXT("1")));

	TestEqual(
		TEXT("Removing the last digit clears input"),
		Input.HandleBackspace(),
		EBlendViewNumericBackspaceResult::Cleared);
	TestFalse(TEXT("Cleared input is inactive"), Input.IsActive());
	TestTrue(TEXT("Cleared buffer is empty"), Input.GetBuffer().IsEmpty());
	return true;
}

#endif
