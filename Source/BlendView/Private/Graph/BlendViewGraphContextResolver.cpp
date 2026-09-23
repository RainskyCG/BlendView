// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Graph/BlendViewGraphContextResolver.h"

#include "EdGraphNode_Comment.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "Layout/WidgetPath.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"

namespace
{
	const FName BlendViewGraphPanelWidgetType(TEXT("SGraphPanel"));
	const FName BlendViewGraphEditorWidgetType(TEXT("SGraphEditor"));
	const FName BlendViewOverlayWidgetType(TEXT("SOverlay"));

	bool IsBlendViewGraphNodeWidgetType(const FName WidgetType)
	{
		return WidgetType.ToString().Contains(TEXT("SGraphNode"));
	}

	bool IsBlendViewTextInputWidgetType(const FName WidgetType)
	{
		const FString WidgetTypeName = WidgetType.ToString();
		return WidgetTypeName.Contains(TEXT("EditableText")) ||
			WidgetTypeName.Contains(TEXT("SearchBox")) ||
			WidgetTypeName.Contains(TEXT("SuggestionTextBox"));
	}
}

bool FBlendViewGraphContext::HasBlockingNodeUnderCursor() const
{
	return NodeUnderCursor && !NodeUnderCursor->IsA<UEdGraphNode_Comment>();
}

FBlendViewGraphContext FBlendViewGraphContextResolver::ResolveUnderCursor()
{
	FBlendViewGraphContext Result;
	if (!FSlateApplication::IsInitialized())
	{
		return Result;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	const FWidgetPath WidgetPath = SlateApplication.LocateWindowUnderMouse(
		SlateApplication.GetCursorPos(),
		SlateApplication.GetInteractiveTopLevelWindows(),
		false);
	for (int32 Index = WidgetPath.Widgets.Num() - 1; Index >= 0; --Index)
	{
		const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[Index].Widget;
		const FName WidgetType = Widget->GetType();
		if (!Result.NodeUnderCursor && IsBlendViewGraphNodeWidgetType(WidgetType))
		{
			Result.NodeUnderCursor = StaticCastSharedRef<SGraphNode>(Widget)->GetNodeObj();
		}
		if (!Result.Panel.IsValid() && WidgetType == BlendViewGraphPanelWidgetType)
		{
			Result.Panel = StaticCastSharedRef<SGraphPanel>(Widget);
		}
		if (!Result.Editor.IsValid() && WidgetType == BlendViewGraphEditorWidgetType)
		{
			Result.Editor = StaticCastSharedRef<SGraphEditor>(Widget);
		}
	}
	return Result;
}

TSharedPtr<SOverlay> FBlendViewGraphContextResolver::FindHostOverlay(
	const TSharedPtr<SGraphPanel>& Panel)
{
	if (!Panel.IsValid() || !FSlateApplication::IsInitialized())
	{
		return nullptr;
	}

	FWidgetPath WidgetPath;
	if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(
		Panel.ToSharedRef(),
		WidgetPath,
		EVisibility::All))
	{
		return nullptr;
	}

	for (int32 Index = WidgetPath.Widgets.Num() - 2; Index >= 0; --Index)
	{
		const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[Index].Widget;
		if (Widget->GetType() == BlendViewOverlayWidgetType)
		{
			return StaticCastSharedRef<SOverlay>(Widget);
		}
	}
	return nullptr;
}

TSharedPtr<SWindow> FBlendViewGraphContextResolver::FindHostWindow(
	const TSharedPtr<SGraphPanel>& Panel)
{
	return Panel.IsValid() && FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindWidgetWindow(Panel.ToSharedRef())
		: nullptr;
}

bool FBlendViewGraphContextResolver::IsTextInputFocused()
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	while (FocusedWidget.IsValid())
	{
		if (IsBlendViewTextInputWidgetType(FocusedWidget->GetType()))
		{
			return true;
		}
		FocusedWidget = FocusedWidget->GetParentWidget();
	}
	return false;
}
