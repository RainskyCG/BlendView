// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SGraphEditor;
class SGraphPanel;
class SOverlay;
class SWindow;
class UEdGraphNode;

struct FBlendViewGraphContext
{
	TSharedPtr<SGraphPanel> Panel;
	TSharedPtr<SGraphEditor> Editor;
	UEdGraphNode* NodeUnderCursor = nullptr;

	bool HasBlockingNodeUnderCursor() const;
};

class FBlendViewGraphContextResolver
{
public:
	static FBlendViewGraphContext ResolveUnderCursor();
	static TSharedPtr<SOverlay> FindHostOverlay(const TSharedPtr<SGraphPanel>& Panel);
	static TSharedPtr<SWindow> FindHostWindow(const TSharedPtr<SGraphPanel>& Panel);
	static bool IsTextInputFocused();
};
