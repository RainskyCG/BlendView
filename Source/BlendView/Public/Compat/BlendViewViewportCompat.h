// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "Compat/BlendViewEngineVersion.h"
#include "EditorViewportClient.h"
#include "SceneView.h"

namespace BlendViewViewportCompat
{
	inline FVector GetForwardVector(const FEditorViewportClient& ViewportClient)
	{
#if BLENDVIEW_UE_5_7_OR_LATER
		return ViewportClient.GetForwardVector().GetSafeNormal();
#else
		return ViewportClient.GetViewRotation().Vector().GetSafeNormal();
#endif
	}

	inline const FMatrix& GetProjectionMatrix(const FSceneView& View)
	{
#if BLENDVIEW_UE_5_8_OR_LATER
		return View.ViewMatrices.GetViewToClip();
#else
		return View.ViewMatrices.GetProjectionMatrix();
#endif
	}

	inline FViewportCursorLocation MakeCursorLocation(
		const FSceneView* View,
		FEditorViewportClient* ViewportClient,
		const int32 X,
		const int32 Y,
		const bool bClampToBounds)
	{
#if BLENDVIEW_UE_5_7_OR_LATER
		return FViewportCursorLocation(View, ViewportClient, X, Y, bClampToBounds);
#else
		return FViewportCursorLocation(View, ViewportClient, X, Y);
#endif
	}
}
