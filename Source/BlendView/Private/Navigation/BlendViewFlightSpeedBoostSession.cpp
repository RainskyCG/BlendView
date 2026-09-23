// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Navigation/BlendViewFlightSpeedBoostSession.h"

#include "EditorViewportClient.h"
#include "Settings/LevelEditorViewportSettings.h"

namespace
{
#if !BLENDVIEW_UE_5_7_OR_LATER
	int32 FindLegacyCameraSpeedSettingAtOrBelow(
		FEditorViewportClient& ViewportClient,
		const float TargetSpeed)
	{
		int32 BestSetting = 1;
		float BestSpeed = ViewportClient.GetCameraSpeed(BestSetting);

		for (int32 SpeedSetting = 1;
			SpeedSetting <= static_cast<int32>(FEditorViewportClient::MaxCameraSpeeds);
			++SpeedSetting)
		{
			const float CandidateSpeed = ViewportClient.GetCameraSpeed(SpeedSetting);
			if (CandidateSpeed <= TargetSpeed + UE_SMALL_NUMBER &&
				CandidateSpeed >= BestSpeed)
			{
				BestSpeed = CandidateSpeed;
				BestSetting = SpeedSetting;
			}
		}

		return BestSetting;
	}

	void ApplyLegacyCameraSpeedMultiplier(
		FEditorViewportClient& ViewportClient,
		const int32 SavedCameraSpeedSetting,
		const float SavedCameraSpeedScalar,
		const float SpeedMultiplier,
		const bool bNativeShiftBoostActive)
	{
		// UE 5.6 applies its own hard-coded 2x speed boost while Shift is
		// pressed and experimental flight navigation is enabled. Keep that
		// engine path enabled for Shift+WASDQE routing, but compensate the
		// stored viewport speed so the final camera speed matches BlendView's
		// configured multiplier.
		const float NativeShiftBoost = bNativeShiftBoostActive ? 2.0f : 1.0f;
		const float PreEngineMultiplier =
			FMath::Max(SpeedMultiplier / NativeShiftBoost, 0.01f);
		if (PreEngineMultiplier >= 1.0f)
		{
			ViewportClient.SetCameraSpeedSetting(SavedCameraSpeedSetting);
			ViewportClient.SetCameraSpeedScalar(SavedCameraSpeedScalar * PreEngineMultiplier);
			return;
		}

		const float TargetSpeed =
			ViewportClient.GetCameraSpeed(SavedCameraSpeedSetting) *
			SavedCameraSpeedScalar *
			PreEngineMultiplier;
		const int32 SpeedSetting =
			FindLegacyCameraSpeedSettingAtOrBelow(ViewportClient, TargetSpeed);
		const float BaseSpeed = FMath::Max(
			ViewportClient.GetCameraSpeed(SpeedSetting),
			KINDA_SMALL_NUMBER);
		ViewportClient.SetCameraSpeedSetting(SpeedSetting);
		ViewportClient.SetCameraSpeedScalar(FMath::Max(TargetSpeed / BaseSpeed, 1.0f));
	}
#endif
}

void FBlendViewFlightSpeedBoostSession::Activate(
	FEditorViewportClient* InViewportClient,
	const float SpeedMultiplier,
	const bool bNativeShiftBoostActive)
{
	if (!InViewportClient)
	{
		return;
	}
	if (bActive && ViewportClient == InViewportClient)
	{
		ActiveSpeedMultiplier = FMath::Max(SpeedMultiplier, 0.01f);
#if BLENDVIEW_UE_5_7_OR_LATER
		FEditorViewportCameraSpeedSettings UpdatedSettings = SavedCameraSpeedSettings;
		UpdatedSettings.SetCurrentSpeed(
			SavedCameraSpeedSettings.GetCurrentSpeed() * ActiveSpeedMultiplier);
		ViewportClient->SetCameraSpeedSettings(UpdatedSettings);
#else
		ApplyLegacyCameraSpeedMultiplier(
			*ViewportClient,
			SavedCameraSpeedSetting,
			SavedCameraSpeedScalar,
			ActiveSpeedMultiplier,
			bNativeShiftBoostActive);
#endif
		return;
	}

	Deactivate();
	ViewportClient = InViewportClient;
#if BLENDVIEW_UE_5_7_OR_LATER
	SavedCameraSpeedSettings = ViewportClient->GetCameraSpeedSettings();
	ActiveSpeedMultiplier = FMath::Max(SpeedMultiplier, 0.01f);
	FEditorViewportCameraSpeedSettings BoostedSettings = SavedCameraSpeedSettings;
	BoostedSettings.SetCurrentSpeed(
		SavedCameraSpeedSettings.GetCurrentSpeed() * ActiveSpeedMultiplier);
	ViewportClient->SetCameraSpeedSettings(BoostedSettings);
#else
	SavedCameraSpeedSetting = ViewportClient->GetCameraSpeedSetting();
	SavedCameraSpeedScalar = ViewportClient->GetCameraSpeedScalar();
	ActiveSpeedMultiplier = FMath::Max(SpeedMultiplier, 0.01f);
	ApplyLegacyCameraSpeedMultiplier(
		*ViewportClient,
		SavedCameraSpeedSetting,
		SavedCameraSpeedScalar,
		ActiveSpeedMultiplier,
		bNativeShiftBoostActive);
#endif

	if (ULevelEditorViewportSettings* Settings = GetMutableDefault<ULevelEditorViewportSettings>())
	{
		bSavedExperimentalNavigation = Settings->FlightCameraControlExperimentalNavigation;
		bHasSavedExperimentalNavigation = true;
		Settings->FlightCameraControlExperimentalNavigation = true;
	}
	bActive = true;
}

void FBlendViewFlightSpeedBoostSession::Deactivate()
{
	if (!bActive)
	{
		return;
	}

	if (ViewportClient)
	{
#if BLENDVIEW_UE_5_7_OR_LATER
		ViewportClient->SetCameraSpeedSettings(SavedCameraSpeedSettings);
#else
		ViewportClient->SetCameraSpeedSetting(SavedCameraSpeedSetting);
		ViewportClient->SetCameraSpeedScalar(SavedCameraSpeedScalar);
#endif
	}
	if (bHasSavedExperimentalNavigation)
	{
		if (ULevelEditorViewportSettings* Settings = GetMutableDefault<ULevelEditorViewportSettings>())
		{
			Settings->FlightCameraControlExperimentalNavigation = bSavedExperimentalNavigation;
		}
		bHasSavedExperimentalNavigation = false;
	}

	ViewportClient = nullptr;
	ActiveSpeedMultiplier = 1.0f;
	bActive = false;
}
