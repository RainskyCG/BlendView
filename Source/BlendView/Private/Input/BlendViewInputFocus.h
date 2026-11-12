// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateApplication;

namespace BlendViewInputFocus
{
	bool IsKeyboardInputOwnerWidgetType(FName WidgetType);
	bool IsKeyboardInputOwnedByFocusedWidget();
	bool IsKeyboardInputOwnedByWidgetAtPosition(
		FSlateApplication& SlateApplication,
		const FVector2D& ScreenPosition);
}
