// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

enum class EBlendViewNumericBackspaceResult : uint8
{
	Ignored,
	Updated,
	Cleared
};

class FBlendViewNumericInput
{
public:
	bool HandleKey(const FKey& Key);
	EBlendViewNumericBackspaceResult HandleBackspace();
	void Clear();

	bool IsActive() const { return bActive; }
	double GetValue() const;
	const FString& GetBuffer() const { return Buffer; }

private:
	bool bActive = false;
	FString Buffer;
};
