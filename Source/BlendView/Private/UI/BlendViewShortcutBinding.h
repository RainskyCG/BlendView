// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUICommandInfo;
struct FInputChord;

namespace BlendViewShortcutBinding
{
	bool HasCustomPrimaryShortcut(const TSharedPtr<FUICommandInfo>& CommandInfo);
	bool AssignPrimaryShortcut(const TSharedRef<FUICommandInfo>& CommandInfo, const FInputChord& NewChord, FText& OutErrorText);
	void ResetPrimaryShortcut(const TSharedRef<FUICommandInfo>& CommandInfo);
}
