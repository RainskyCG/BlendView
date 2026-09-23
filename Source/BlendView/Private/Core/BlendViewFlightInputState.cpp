// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Core/BlendViewFlightInputState.h"

#include "InputCoreTypes.h"

void FBlendViewFlightInputState::Update(const FBlendViewInputEvent& Event)
{
	if (Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::RightMouseButton)
	{
		bRightMouseDown = true;
	}
	else if (Event.Type == EBlendViewInputEventType::MouseUp && Event.Key == EKeys::RightMouseButton)
	{
		bRightMouseDown = false;
	}

	if (Event.Type == EBlendViewInputEventType::KeyDown && IsShiftKey(Event.Key))
	{
		bShiftDown = true;
	}
	else if (Event.Type == EBlendViewInputEventType::KeyUp && IsShiftKey(Event.Key))
	{
		bShiftDown = Event.bShiftDown;
	}
	if (Event.Type == EBlendViewInputEventType::KeyDown && IsAltKey(Event.Key))
	{
		bAltDown = true;
	}
	else if (Event.Type == EBlendViewInputEventType::KeyUp && IsAltKey(Event.Key))
	{
		bAltDown = Event.bAltDown;
	}
	else if ((Event.Type == EBlendViewInputEventType::KeyDown ||
		Event.Type == EBlendViewInputEventType::KeyUp) &&
		!IsShiftKey(Event.Key) &&
		!IsAltKey(Event.Key))
	{
		bShiftDown = Event.bShiftDown;
		bAltDown = Event.bAltDown;
	}
	else if (IsPointerEvent(Event))
	{
		bShiftDown = Event.bShiftDown;
		bAltDown = Event.bAltDown;
	}

	if (Event.Type == EBlendViewInputEventType::KeyDown ||
		Event.Type == EBlendViewInputEventType::KeyUp)
	{
		SetNavigationKeyState(Event.Key, Event.Type == EBlendViewInputEventType::KeyDown);
	}
}

void FBlendViewFlightInputState::Reset()
{
	bRightMouseDown = false;
	bShiftDown = false;
	bAltDown = false;
	ClearNavigationInput();
}

void FBlendViewFlightInputState::SetRightMouseDown(const bool bInRightMouseDown)
{
	bRightMouseDown = bInRightMouseDown;
	if (!bRightMouseDown)
	{
		ClearNavigationInput();
	}
}

void FBlendViewFlightInputState::SetModifierState(const bool bInShiftDown, const bool bInAltDown)
{
	bShiftDown = bInShiftDown;
	bAltDown = bInAltDown;
}

void FBlendViewFlightInputState::SetNavigationKeyState(const FKey& Key, const bool bPressed)
{
	if (Key == EKeys::W)
	{
		bForwardDown = bPressed;
	}
	else if (Key == EKeys::S)
	{
		bBackwardDown = bPressed;
	}
	else if (Key == EKeys::D)
	{
		bRightDown = bPressed;
	}
	else if (Key == EKeys::A)
	{
		bLeftDown = bPressed;
	}
	else if (Key == EKeys::E)
	{
		bWorldUpDown = bPressed;
	}
	else if (Key == EKeys::Q)
	{
		bWorldDownDown = bPressed;
	}
}

void FBlendViewFlightInputState::ClearNavigationInput()
{
	bForwardDown = false;
	bBackwardDown = false;
	bRightDown = false;
	bLeftDown = false;
	bWorldUpDown = false;
	bWorldDownDown = false;
}

void FBlendViewFlightInputState::AppendPressedNavigationKeys(TArray<FKey>& OutKeys) const
{
	if (bForwardDown)
	{
		OutKeys.Add(EKeys::W);
	}
	if (bBackwardDown)
	{
		OutKeys.Add(EKeys::S);
	}
	if (bRightDown)
	{
		OutKeys.Add(EKeys::D);
	}
	if (bLeftDown)
	{
		OutKeys.Add(EKeys::A);
	}
	if (bWorldUpDown)
	{
		OutKeys.Add(EKeys::E);
	}
	if (bWorldDownDown)
	{
		OutKeys.Add(EKeys::Q);
	}
}

bool FBlendViewFlightInputState::HasNavigationInput() const
{
	return bForwardDown ||
		bBackwardDown ||
		bRightDown ||
		bLeftDown ||
		bWorldUpDown ||
		bWorldDownDown;
}

float FBlendViewFlightInputState::GetForwardBackwardImpulse() const
{
	return (bForwardDown ? 1.0f : 0.0f) - (bBackwardDown ? 1.0f : 0.0f);
}

float FBlendViewFlightInputState::GetRightLeftImpulse() const
{
	return (bRightDown ? 1.0f : 0.0f) - (bLeftDown ? 1.0f : 0.0f);
}

float FBlendViewFlightInputState::GetWorldUpDownImpulse() const
{
	return (bWorldUpDown ? 1.0f : 0.0f) - (bWorldDownDown ? 1.0f : 0.0f);
}

float FBlendViewFlightInputState::GetSpeedMultiplier(const float BaseMultiplier) const
{
	const float SafeMultiplier = FMath::Max(BaseMultiplier, 1.0f);
	if (!bRightMouseDown || bShiftDown == bAltDown)
	{
		return 1.0f;
	}
	return bShiftDown ? SafeMultiplier : 1.0f / SafeMultiplier;
}

bool FBlendViewFlightInputState::IsNavigationKey(const FKey& Key)
{
	return Key == EKeys::W ||
		Key == EKeys::A ||
		Key == EKeys::S ||
		Key == EKeys::D ||
		Key == EKeys::Q ||
		Key == EKeys::E;
}

bool FBlendViewFlightInputState::IsShiftKey(const FKey& Key)
{
	return Key == EKeys::LeftShift || Key == EKeys::RightShift;
}

bool FBlendViewFlightInputState::IsAltKey(const FKey& Key)
{
	return Key == EKeys::LeftAlt || Key == EKeys::RightAlt;
}

bool FBlendViewFlightInputState::IsPointerEvent(const FBlendViewInputEvent& Event)
{
	return Event.Type == EBlendViewInputEventType::MouseMove ||
		Event.Type == EBlendViewInputEventType::MouseDown ||
		Event.Type == EBlendViewInputEventType::MouseUp ||
		Event.Type == EBlendViewInputEventType::MouseDoubleClick ||
		Event.Type == EBlendViewInputEventType::MouseWheel;
}
