// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

enum class EBlendViewInputEventType : uint8
{
	KeyDown,
	KeyUp,
	MouseMove,
	MouseDown,
	MouseUp,
	MouseDoubleClick,
	MouseWheel
};

struct FBlendViewInputEvent
{
	EBlendViewInputEventType Type = EBlendViewInputEventType::KeyDown;
	FKey Key;
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	FVector2D CursorDelta = FVector2D::ZeroVector;
	bool bShiftDown = false;
	bool bControlDown = false;
	bool bAltDown = false;
	bool bCommandDown = false;
	bool bIsRepeat = false;

	bool IsModified() const
	{
		return bShiftDown || bControlDown || bAltDown || bCommandDown;
	}
};

enum class EBlendViewInputResult : uint8
{
	PassThrough,
	Handled
};
