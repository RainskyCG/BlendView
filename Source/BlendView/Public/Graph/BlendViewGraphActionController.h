// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewInputTypes.h"

class SGraphPanel;
class UEdGraphNode;

class FBlendViewGraphActionController
{
public:
	bool TryFrameGraphUnderCursor(const FBlendViewInputEvent& Event) const;
	bool TryToggleMaterialPreviewUnderCursor(const FBlendViewInputEvent& Event) const;
	bool TryDeleteAndReconnectUnderCursor(const FBlendViewInputEvent& Event) const;
	bool IsTextInputFocused() const;

private:
	bool ReconnectAndDeleteK2Nodes(const TArray<UEdGraphNode*>& SelectedNodes) const;
	bool ReconnectAndDeleteMaterialNodes(const TArray<UEdGraphNode*>& SelectedNodes) const;
};
