// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UCombinedTransformGizmo;
class UTransformProxy;

class FBlendViewModelingEditPivotBridge final
{
public:
	struct FTarget
	{
		UTransformProxy* TransformProxy = nullptr;
		UCombinedTransformGizmo* TransformGizmo = nullptr;
	};

	static bool FindActiveTarget(FTarget& OutTarget);
	static UTransformProxy* FindActiveTransformProxy();
	static bool StartToolIfNeeded();

	static void CacheInitialTransform(UTransformProxy* TransformProxy);
	static void EnsureInitialTransformCached(UTransformProxy* TransformProxy);

	static bool ResetActiveLocation();
	static bool ResetActiveRotation();
};
