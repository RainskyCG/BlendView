// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "Compat/BlendViewEngineVersion.h"
#include "CoreMinimal.h"

#if BLENDVIEW_UE_5_7_OR_LATER
#include "Settings/EditorViewportSettings.h"
#endif

class FEditorViewportClient;

class FBlendViewFlightSpeedBoostSession
{
public:
	void Activate(
		FEditorViewportClient* ViewportClient,
		float SpeedMultiplier,
		bool bNativeShiftBoostActive = false);
	void Deactivate();

	bool IsActive() const { return bActive; }

private:
#if BLENDVIEW_UE_5_7_OR_LATER
	FEditorViewportCameraSpeedSettings SavedCameraSpeedSettings;
#else
	int32 SavedCameraSpeedSetting = 4;
	float SavedCameraSpeedScalar = 1.0f;
#endif
	FEditorViewportClient* ViewportClient = nullptr;
	bool bSavedExperimentalNavigation = false;
	bool bHasSavedExperimentalNavigation = false;
	float ActiveSpeedMultiplier = 1.0f;
	bool bActive = false;
};
