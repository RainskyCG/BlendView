// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Graph/BlendViewGraphActionController.h"

#include "BlendViewCommands.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Graph/BlendViewGraphContextResolver.h"
#include "Graph/BlendViewGraphCommands.h"
#include "IMaterialEditor.h"
#include "K2Node.h"
#include "K2Node_Knot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialEditorUtilities.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "ScopedTransaction.h"
#include "SGraphPanel.h"

bool FBlendViewGraphActionController::TryFrameGraphUnderCursor(const FBlendViewInputEvent& Event) const
{
	if (!FBlendViewCommands::IsRegistered() ||
		!FBlendViewCommands::Get().FrameSelected.IsValid() ||
		!FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewCommands::Get().FrameSelected))
	{
		return false;
	}

	const TSharedPtr<SGraphPanel> Panel =
		FBlendViewGraphContextResolver::ResolveUnderCursor().Panel;
	if (!Panel.IsValid())
	{
		return false;
	}

	const bool bOnlySelection = !Panel->GetSelectedGraphNodes().IsEmpty();
	Panel->ZoomToFit(bOnlySelection);
	return true;
}

bool FBlendViewGraphActionController::TryToggleMaterialPreviewUnderCursor(const FBlendViewInputEvent& Event) const
{
	if (Event.Type != EBlendViewInputEventType::MouseDown ||
		Event.Key != EKeys::LeftMouseButton ||
		!Event.bControlDown ||
		!Event.bShiftDown ||
		Event.bAltDown ||
		Event.bCommandDown)
	{
		return false;
	}

	UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(
		FBlendViewGraphContextResolver::ResolveUnderCursor().NodeUnderCursor);
	if (!MaterialNode)
	{
		return false;
	}

	TSharedPtr<IMaterialEditor> MaterialEditor =
		FMaterialEditorUtilities::GetIMaterialEditorForObject(MaterialNode->GetGraph());
	if (!MaterialEditor.IsValid())
	{
		return false;
	}

	MaterialEditor->PreviewNode(MaterialNode->bIsPreviewExpression ? nullptr : MaterialNode);
	return true;
}

bool FBlendViewGraphActionController::TryDeleteAndReconnectUnderCursor(const FBlendViewInputEvent& Event) const
{
	if (IsTextInputFocused() ||
		!FBlendViewGraphCommands::IsRegistered() ||
		!FBlendViewGraphCommands::Get().DeleteAndReconnect.IsValid() ||
		!FBlendViewCommands::MatchesInputEvent(Event, *FBlendViewGraphCommands::Get().DeleteAndReconnect))
	{
		return false;
	}

	const TSharedPtr<SGraphPanel> Panel =
		FBlendViewGraphContextResolver::ResolveUnderCursor().Panel;
	if (!Panel.IsValid() || !Panel->IsGraphEditable())
	{
		return false;
	}

	const TArray<UEdGraphNode*> SelectedNodes = Panel->GetSelectedGraphNodes();
	if (SelectedNodes.IsEmpty())
	{
		return false;
	}

	return ReconnectAndDeleteK2Nodes(SelectedNodes) ||
		ReconnectAndDeleteMaterialNodes(SelectedNodes);
}

bool FBlendViewGraphActionController::IsTextInputFocused() const
{
	return FBlendViewGraphContextResolver::IsTextInputFocused();
}

bool FBlendViewGraphActionController::ReconnectAndDeleteK2Nodes(const TArray<UEdGraphNode*>& SelectedNodes) const
{
	TArray<UK2Node*> K2Nodes;
	K2Nodes.Reserve(SelectedNodes.Num());
	for (UEdGraphNode* Node : SelectedNodes)
	{
		UK2Node* K2Node = Cast<UK2Node>(Node);
		if (!IsValid(K2Node) || !K2Node->CanUserDeleteNode())
		{
			return false;
		}
		K2Nodes.Add(K2Node);
	}
	if (K2Nodes.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(K2Nodes[0]);
	if (!Blueprint)
	{
		return false;
	}
	for (UK2Node* Node : K2Nodes)
	{
		if (FBlueprintEditorUtils::FindBlueprintForNode(Node) != Blueprint)
		{
			return false;
		}
	}

	const FScopedTransaction Transaction(NSLOCTEXT("BlendView", "DeleteAndReconnectGraphNodes", "BlendView Delete and Reconnect Nodes"));
	bool bStructuralChange = false;
	for (UK2Node* Node : K2Nodes)
	{
		if (!IsValid(Node))
		{
			continue;
		}

		Node->Modify();
		bStructuralChange |= Node->NodeCausesStructuralBlueprintChange();

		UEdGraphPin* ExecPin = nullptr;
		UEdGraphPin* ThenPin = nullptr;
		if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(Node))
		{
			ExecPin = KnotNode->GetInputPin();
			ThenPin = KnotNode->GetOutputPin();
		}
		else if (!Node->IsNodePure())
		{
			ExecPin = Node->GetExecPin();
			ThenPin = Node->FindPin(UEdGraphSchema_K2::PN_Then);
		}

		if (ExecPin && ThenPin)
		{
			const TArray<UEdGraphPin*> IncomingPins = ExecPin->LinkedTo;
			const TArray<UEdGraphPin*> OutgoingPins = ThenPin->LinkedTo;
			for (UEdGraphPin* IncomingPin : IncomingPins)
			{
				if (!IncomingPin)
				{
					continue;
				}
				for (UEdGraphPin* OutgoingPin : OutgoingPins)
				{
					if (OutgoingPin)
					{
						IncomingPin->MakeLinkTo(OutgoingPin);
					}
				}
			}
		}

		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
	}

	if (bStructuralChange)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	return true;
}

bool FBlendViewGraphActionController::ReconnectAndDeleteMaterialNodes(const TArray<UEdGraphNode*>& SelectedNodes) const
{
	TArray<UMaterialGraphNode*> MaterialNodes;
	MaterialNodes.Reserve(SelectedNodes.Num());
	for (UEdGraphNode* Node : SelectedNodes)
	{
		UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Node);
		if (!IsValid(MaterialNode) || !MaterialNode->CanUserDeleteNode())
		{
			return false;
		}
		MaterialNodes.Add(MaterialNode);
	}
	if (MaterialNodes.IsEmpty())
	{
		return false;
	}

	UEdGraph* Graph = MaterialNodes[0]->GetGraph();
	const UEdGraphSchema* Schema = Graph ? Graph->GetSchema() : nullptr;
	if (!Graph || !Schema)
	{
		return false;
	}

	TSet<UEdGraphNode*> SelectedNodeSet;
	for (UMaterialGraphNode* Node : MaterialNodes)
	{
		if (Node->GetGraph() != Graph)
		{
			return false;
		}
		SelectedNodeSet.Add(Node);
	}

	struct FPendingMaterialReconnect
	{
		UEdGraphPin* SourcePin = nullptr;
		UEdGraphPin* TargetPin = nullptr;
		UEdGraphPin* DeletedOutputPin = nullptr;
	};

	TArray<FPendingMaterialReconnect> PendingReconnects;
	for (UMaterialGraphNode* Node : MaterialNodes)
	{
		TArray<UEdGraphPin*> ExternalSources;
		for (UEdGraphPin* InputPin : Node->Pins)
		{
			if (!InputPin || InputPin->Direction != EGPD_Input)
			{
				continue;
			}
			for (UEdGraphPin* LinkedPin : InputPin->LinkedTo)
			{
				if (LinkedPin && !SelectedNodeSet.Contains(LinkedPin->GetOwningNode()))
				{
					ExternalSources.AddUnique(LinkedPin);
				}
			}
		}

		for (UEdGraphPin* OutputPin : Node->Pins)
		{
			if (!OutputPin || OutputPin->Direction != EGPD_Output)
			{
				continue;
			}
			for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
			{
				if (!LinkedPin || SelectedNodeSet.Contains(LinkedPin->GetOwningNode()))
				{
					continue;
				}
				for (UEdGraphPin* SourcePin : ExternalSources)
				{
					PendingReconnects.Add({SourcePin, LinkedPin, OutputPin});
				}
			}
		}
	}

	const FScopedTransaction Transaction(NSLOCTEXT("BlendView", "DeleteAndMergeMaterialNodes", "BlendView Delete and Merge Material Nodes"));
	Graph->Modify();
	for (UMaterialGraphNode* Node : MaterialNodes)
	{
		Node->Modify();
	}

	for (const FPendingMaterialReconnect& Reconnect : PendingReconnects)
	{
		if (!Reconnect.SourcePin || !Reconnect.TargetPin || !Reconnect.DeletedOutputPin)
		{
			continue;
		}
		Reconnect.TargetPin->BreakLinkTo(Reconnect.DeletedOutputPin);
		Schema->TryCreateConnection(Reconnect.SourcePin, Reconnect.TargetPin);
	}

	TArray<UEdGraphNode*> NodesToDelete;
	NodesToDelete.Reserve(MaterialNodes.Num());
	for (UMaterialGraphNode* Node : MaterialNodes)
	{
		NodesToDelete.Add(Node);
	}
	FMaterialEditorUtilities::DeleteNodes(Graph, NodesToDelete);
	Graph->NotifyGraphChanged();
	return true;
}
