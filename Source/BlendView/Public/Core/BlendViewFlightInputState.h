// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewInputTypes.h"

class FBlendViewFlightInputState
{
public:
	void Update(const FBlendViewInputEvent& Event);
	void Reset();
	void SetRightMouseDown(bool bInRightMouseDown);
	void SetModifierState(bool bInShiftDown, bool bInAltDown);
	void SetNavigationKeyState(const FKey& Key, bool bPressed);
	void ClearNavigationInput();
	void AppendPressedNavigationKeys(TArray<FKey>& OutKeys) const;

	bool IsRightMouseDown() const { return bRightMouseDown; }
	bool IsShiftDown() const { return bShiftDown; }
	bool IsAltDown() const { return bAltDown; }
	bool HasNavigationInput() const;
	float GetForwardBackwardImpulse() const;
	float GetRightLeftImpulse() const;
	float GetWorldUpDownImpulse() const;
	bool ShouldModifySpeed() const { return bRightMouseDown && bShiftDown != bAltDown; }
	float GetSpeedMultiplier(float BaseMultiplier) const;
	static bool IsNavigationKey(const FKey& Key);
	static bool IsAltKey(const FKey& Key);

private:
	static bool IsShiftKey(const FKey& Key);
	static bool IsPointerEvent(const FBlendViewInputEvent& Event);

	bool bRightMouseDown = false;
	bool bShiftDown = false;
	bool bAltDown = false;
	bool bForwardDown = false;
	bool bBackwardDown = false;
	bool bRightDown = false;
	bool bLeftDown = false;
	bool bWorldUpDown = false;
	bool bWorldDownDown = false;
};
