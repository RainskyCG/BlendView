// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Input/BlendViewInputFocus.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewInputFocusWidgetTypeTest,
	"BlendView.Input.KeyboardFocusOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewInputFocusWidgetTypeTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Editable text owns keyboard input"),
		BlendViewInputFocus::IsKeyboardInputOwnerWidgetType(TEXT("SEditableText")));
	TestTrue(
		TEXT("Search boxes own keyboard input"),
		BlendViewInputFocus::IsKeyboardInputOwnerWidgetType(TEXT("SSearchBox")));
	TestTrue(
		TEXT("Fab web browser view owns keyboard input"),
		BlendViewInputFocus::IsKeyboardInputOwnerWidgetType(TEXT("SWebBrowserView")));
	TestTrue(
		TEXT("Fab web browser container owns keyboard input"),
		BlendViewInputFocus::IsKeyboardInputOwnerWidgetType(TEXT("SWebBrowser")));
	TestFalse(
		TEXT("Level viewport remains available to BlendView"),
		BlendViewInputFocus::IsKeyboardInputOwnerWidgetType(TEXT("SLevelViewport")));
	TestFalse(
		TEXT("Graph panel remains available to BlendView"),
		BlendViewInputFocus::IsKeyboardInputOwnerWidgetType(TEXT("SGraphPanel")));
	return true;
}

#endif
