// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace BlendViewPopupStyle
{
	TSharedRef<SWidget> MakeFloatingMenuShell(const TSharedRef<SWidget>& Content);
	TSharedRef<SWidget> MakeMenuContentShell(const TSharedRef<SWidget>& Content, FMargin Padding = FMargin(7.0f, 7.0f, 7.0f, 8.0f));
	TSharedRef<SWidget> MakeMenuShell(const TSharedRef<SWidget>& Content, FMargin Padding = FMargin(7.0f, 7.0f, 7.0f, 8.0f));
	TSharedRef<SWidget> MakeSeparator();
}
