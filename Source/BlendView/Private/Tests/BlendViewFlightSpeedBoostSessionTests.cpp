// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Compat/BlendViewEngineVersion.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Misc/AutomationTest.h"
#include "Navigation/BlendViewFlightSpeedBoostSession.h"
#include "Settings/LevelEditorViewportSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	float GetBlendViewTestCameraSpeed(FLevelEditorViewportClient* ViewportClient)
	{
#if BLENDVIEW_UE_5_7_OR_LATER
		return ViewportClient->GetCameraSpeedSettings().GetCurrentSpeed();
#else
		return ViewportClient->GetCameraSpeed(ViewportClient->GetCameraSpeedSetting()) *
			ViewportClient->GetCameraSpeedScalar();
#endif
	}

	float GetBlendViewExpectedCameraSpeed(
		FLevelEditorViewportClient* ViewportClient,
		const float OriginalCameraSpeed,
		const float SpeedMultiplier)
	{
#if BLENDVIEW_UE_5_7_OR_LATER
		return OriginalCameraSpeed * SpeedMultiplier;
#else
		return FMath::Max(
			OriginalCameraSpeed * SpeedMultiplier,
			ViewportClient->GetCameraSpeed(1));
#endif
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewFlightSpeedBoostSessionTest,
	"BlendView.Input.FlightBoost.SessionRestoresEditorState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewFlightSpeedBoostSessionTest::RunTest(const FString& Parameters)
{
	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (!ViewportClient && GEditor)
	{
		const TArray<FLevelEditorViewportClient*>& ViewportClients = GEditor->GetLevelViewportClients();
		ViewportClient = ViewportClients.IsEmpty() ? nullptr : ViewportClients[0];
	}
	ULevelEditorViewportSettings* Settings = GetMutableDefault<ULevelEditorViewportSettings>();
	if (!ViewportClient || !Settings)
	{
		AddWarning(TEXT("Skipped because a level viewport or viewport settings are unavailable."));
		return true;
	}

	const float OriginalCameraSpeed = GetBlendViewTestCameraSpeed(ViewportClient);
#if !BLENDVIEW_UE_5_7_OR_LATER
	const int32 OriginalCameraSpeedSetting = ViewportClient->GetCameraSpeedSetting();
	const float OriginalCameraSpeedScalar = ViewportClient->GetCameraSpeedScalar();
#endif
	const bool bOriginalExperimentalNavigation = Settings->FlightCameraControlExperimentalNavigation;

	FBlendViewFlightSpeedBoostSession Session;
	Session.Activate(ViewportClient, 1.0f);
	TestTrue(TEXT("Boost session is active"), Session.IsActive());
	TestTrue(
		TEXT("UE Shift flight navigation is enabled for the whole RMB session"),
		Settings->FlightCameraControlExperimentalNavigation);
	TestTrue(
		TEXT("Base RMB session preserves viewport camera speed"),
		FMath::IsNearlyEqual(
			GetBlendViewTestCameraSpeed(ViewportClient),
			OriginalCameraSpeed));

	Session.Activate(ViewportClient, 5.0f);
	TestTrue(
		TEXT("Viewport camera speed is multiplied during boost"),
		FMath::IsNearlyEqual(
			GetBlendViewTestCameraSpeed(ViewportClient),
			OriginalCameraSpeed * 5.0f));
	Session.Activate(ViewportClient, 1.0f);
	TestTrue(
		TEXT("Releasing Shift restores speed without ending the RMB session"),
		FMath::IsNearlyEqual(
			GetBlendViewTestCameraSpeed(ViewportClient),
			OriginalCameraSpeed));

	Session.Activate(ViewportClient, 1.0f / 5.0f);
	TestTrue(
		TEXT("Viewport camera speed is divided during slow mode"),
		FMath::IsNearlyEqual(
			GetBlendViewTestCameraSpeed(ViewportClient),
			GetBlendViewExpectedCameraSpeed(
				ViewportClient,
				OriginalCameraSpeed,
				1.0f / 5.0f)));

	Session.Deactivate();
	TestFalse(TEXT("Boost session is inactive after release"), Session.IsActive());
	TestTrue(
		TEXT("Viewport camera speed is restored exactly"),
		FMath::IsNearlyEqual(
			GetBlendViewTestCameraSpeed(ViewportClient),
			OriginalCameraSpeed));
#if !BLENDVIEW_UE_5_7_OR_LATER
	TestEqual(
		TEXT("Legacy camera speed setting is restored"),
		ViewportClient->GetCameraSpeedSetting(),
		OriginalCameraSpeedSetting);
	TestTrue(
		TEXT("Legacy camera speed scalar is restored"),
		FMath::IsNearlyEqual(
			ViewportClient->GetCameraSpeedScalar(),
			OriginalCameraSpeedScalar));
#endif
	TestEqual(
		TEXT("Experimental navigation setting is restored"),
		Settings->FlightCameraControlExperimentalNavigation,
		bOriginalExperimentalNavigation);
	return true;
}

#endif
