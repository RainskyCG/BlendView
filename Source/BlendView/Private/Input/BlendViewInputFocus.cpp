// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Input/BlendViewInputFocus.h"

#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWidget.h"

namespace BlendViewInputFocus
{
	bool IsKeyboardInputOwnerWidgetType(const FName WidgetType)
	{
		const FString WidgetTypeName = WidgetType.ToString();
		return WidgetTypeName.Contains(TEXT("EditableText")) ||
			WidgetTypeName.Contains(TEXT("SearchBox")) ||
			WidgetTypeName.Contains(TEXT("SuggestionTextBox")) ||
			WidgetTypeName.Contains(TEXT("WebBrowser"));
	}

	bool IsKeyboardInputOwnedByFocusedWidget()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
		while (FocusedWidget.IsValid())
		{
			if (IsKeyboardInputOwnerWidgetType(FocusedWidget->GetType()))
			{
				return true;
			}
			FocusedWidget = FocusedWidget->GetParentWidget();
		}
		return false;
	}

	bool IsKeyboardInputOwnedByWidgetAtPosition(
		FSlateApplication& SlateApplication,
		const FVector2D& ScreenPosition)
	{
		const FWidgetPath WidgetPath = SlateApplication.LocateWindowUnderMouse(
			ScreenPosition,
			SlateApplication.GetInteractiveTopLevelWindows(),
			false);
		for (int32 Index = 0; Index < WidgetPath.Widgets.Num(); ++Index)
		{
			if (IsKeyboardInputOwnerWidgetType(WidgetPath.Widgets[Index].Widget->GetType()))
			{
				return true;
			}
		}
		return false;
	}
}
