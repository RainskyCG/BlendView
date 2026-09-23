// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Graph/BlendViewGraphTransformController.h"

#include "Algo/AllOf.h"
#include "BlendViewCommands.h"
#include "BlendViewSettings.h"
#include "Core/BlendViewSessionState.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphUtilities.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Fonts/FontMeasure.h"
#include "GraphEditor.h"
#include "Graph/BlendViewGraphContextResolver.h"
#include "Graph/BlendViewGraphCommands.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Localization/BlendViewLocalization.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "MaterialEditorUtilities.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "ScopedTransaction.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "SNodePanel.h"
#include "UI/BlendViewGuideDrawing.h"
#include "UI/BlendViewStatusBarPresenter.h"
#include "UI/BlendViewTransformCursorRenderer.h"
#include "UI/BlendViewTransformValueFormatter.h"
#include "Tools/BlendViewTransformPrecision.h"
#include "Tools/BlendViewTransformStatusBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	constexpr float PrecisionFactor = 0.1f;
	constexpr float RotationPrecisionFactor = 1.0f / 30.0f;
	constexpr float RotationSnapDegrees = 5.0f;
	constexpr float RotationPrecisionSnapDegrees = 1.0f;
	constexpr float ScaleSnapStep = 0.1f;
	constexpr float ScalePrecisionSnapStep = 0.01f;
	constexpr float GraphValueHorizontalInsetPixels = 10.0f;
	constexpr float GraphValueTopGapPixels = 6.0f;
	const FLinearColor GraphTransformValueFallbackTextColor =
		FLinearColor::FromSRGBColor(FColor(0x7B, 0x7B, 0x7B, 0xFF));

	FLinearColor GetGraphTransformValueTextColor()
	{
		const FSlateColor& TitleTextColor =
			FAppStyle::GetWidgetStyle<FTextBlockStyle>(TEXT("GraphBreadcrumbButtonText")).ColorAndOpacity;
		return TitleTextColor.IsColorSpecified()
			? TitleTextColor.GetSpecifiedColor()
			: GraphTransformValueFallbackTextColor;
	}

	bool IsGraphControlModifierKey(const FKey& Key)
	{
		return Key == EKeys::LeftControl || Key == EKeys::RightControl;
	}

	void RestoreMaterialNodesAfterCopy(const TSet<UObject*>& Nodes)
	{
		for (UObject* Object : Nodes)
		{
			if (UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Object))
			{
				MaterialNode->PostCopyNode();
			}
			else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(Object))
			{
				CommentNode->PostCopyNode();
			}
		}
	}

	void DeleteGraphNodes(UEdGraph* Graph, const TArray<UEdGraphNode*>& Nodes)
	{
		if (!Graph || Nodes.IsEmpty())
		{
			return;
		}

		const bool bAllMaterialNodes = Algo::AllOf(
			Nodes,
			[](const UEdGraphNode* Node)
			{
				return Node &&
					(Node->IsA<UMaterialGraphNode>() || Node->IsA<UMaterialGraphNode_Comment>());
			});
		if (bAllMaterialNodes)
		{
			FMaterialEditorUtilities::DeleteNodes(Graph, Nodes);
			return;
		}

		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			bool bStructuralChange = false;
			for (UEdGraphNode* Node : Nodes)
			{
				if (!IsValid(Node) || Node->GetGraph() != Graph)
				{
					continue;
				}
				if (const UK2Node* K2Node = Cast<UK2Node>(Node))
				{
					bStructuralChange |= K2Node->NodeCausesStructuralBlueprintChange();
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
			return;
		}

		for (UEdGraphNode* Node : Nodes)
		{
			if (IsValid(Node) && Node->GetGraph() == Graph)
			{
				Node->Modify();
				Node->DestroyNode();
			}
		}
		Graph->NotifyGraphChanged();
	}

	void RestoreGraphSelection(
		const TSharedPtr<SGraphEditor>& GraphEditor,
		const TArray<TWeakObjectPtr<UEdGraphNode>>& Selection)
	{
		if (!GraphEditor.IsValid())
		{
			return;
		}

		GraphEditor->ClearSelectionSet();
		for (const TWeakObjectPtr<UEdGraphNode>& WeakNode : Selection)
		{
			if (UEdGraphNode* Node = WeakNode.Get())
			{
				GraphEditor->SetNodeSelection(Node, true);
			}
		}
	}

	class SBlendViewGraphGuideOverlay final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlendViewGraphGuideOverlay) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&) {}

		void SetGuide(
			const FVector2f& InStartGraph,
			const FVector2f& InEndScreen,
			const TSharedPtr<SGraphPanel>& InPanel,
			const bool bInVisible)
		{
			StartGraph = InStartGraph;
			EndScreen = InEndScreen;
			TransformPanel = InPanel;
			bVisible = bInVisible;
			Invalidate(EInvalidateWidgetReason::Paint);
		}

		void SetValue(
			const FText& InValueText,
			const TSharedPtr<SWidget>& InValueHost,
			const bool bInUsingEditorFallback)
		{
			ValueText = InValueText;
			ValueHost = InValueHost;
			bUsingEditorFallback = bInUsingEditorFallback;
			Invalidate(EInvalidateWidgetReason::Paint);
		}

		void SetCursor(
			const EBlendViewGraphTransformMode InMode,
			const FVector2f& InPivotGraph,
			const FVector2f& InCursorScreen,
			const TSharedPtr<SGraphPanel>& InPanel,
			const bool bInCursorVisible)
		{
			CursorMode = InMode;
			CursorPivotGraph = InPivotGraph;
			CursorScreen = InCursorScreen;
			TransformPanel = InPanel;
			bCursorVisible = bInCursorVisible;
			Invalidate(EInvalidateWidgetReason::Paint);
		}

	private:
		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D::ZeroVector;
		}

		virtual int32 OnPaint(
			const FPaintArgs& Args,
			const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements,
			int32 LayerId,
			const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled) const override
		{
			int32 MaxLayerId = LayerId;
			if (!ValueText.IsEmpty() && FSlateApplication::IsInitialized())
			{
				if (const TSharedPtr<SWidget> Host = ValueHost.Pin())
				{
					const FGeometry& OverlayInputGeometry = GetTickSpaceGeometry();
					const FGeometry& HostGeometry = Host->GetTickSpaceGeometry();
					const FVector2f HostSize(HostGeometry.GetLocalSize());
					const float HostBottom = bUsingEditorFallback
						? FMath::Min(HostSize.Y, 36.0f)
						: HostSize.Y;
					const FVector2f TitleBottomLeft = OverlayInputGeometry.AbsoluteToLocal(
						HostGeometry.LocalToAbsolute(FVector2f(0.0f, HostBottom)));
					const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
					const TSharedRef<FSlateFontMeasure> FontMeasure =
						FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
					const FVector2D TextSize = FontMeasure->Measure(ValueText, Font);
					const FVector2D TextPosition =
						FVector2D(TitleBottomLeft) +
						FVector2D(GraphValueHorizontalInsetPixels, GraphValueTopGapPixels);
					FSlateDrawElement::MakeText(
						OutDrawElements,
						LayerId + 11,
						AllottedGeometry.ToPaintGeometry(
							TextSize,
							FSlateLayoutTransform(TextPosition)),
						ValueText,
						Font,
						ESlateDrawEffect::None,
						GetGraphTransformValueTextColor());
					MaxLayerId = LayerId + 11;
				}
			}

			if (bVisible)
			{
				if (const TSharedPtr<SGraphPanel> Panel = TransformPanel.Pin())
				{
					const FGeometry& OverlayInputGeometry = GetTickSpaceGeometry();
					const FGeometry& PanelGeometry = Panel->GetTickSpaceGeometry();
					const FVector2f GraphAtPanelOrigin =
						Panel->PanelCoordToGraphCoord(FVector2f::ZeroVector);
					const FVector2f StartPanel =
						(StartGraph - GraphAtPanelOrigin) * Panel->GetZoomAmount();
					const FVector2f Start = OverlayInputGeometry.AbsoluteToLocal(
						PanelGeometry.LocalToAbsolute(StartPanel));
					const FVector2f End = OverlayInputGeometry.AbsoluteToLocal(EndScreen);
					if (!Start.Equals(End, 0.5f))
					{
						MaxLayerId = FMath::Max(
							MaxLayerId,
							BlendViewGuideDrawing::DrawSlateDashedGuide(
								OutDrawElements,
								AllottedGeometry,
								LayerId,
								FVector2D(Start),
								FVector2D(End)));
					}
				}
			}

			if (bCursorVisible)
			{
				if (const TSharedPtr<SGraphPanel> Panel = TransformPanel.Pin())
				{
					const FGeometry& OverlayInputGeometry = GetTickSpaceGeometry();
					const FGeometry& PanelGeometry = Panel->GetTickSpaceGeometry();
					const FVector2f GraphAtPanelOrigin =
						Panel->PanelCoordToGraphCoord(FVector2f::ZeroVector);
					const FVector2f PivotPanel =
						(CursorPivotGraph - GraphAtPanelOrigin) * Panel->GetZoomAmount();
					const FVector2f PivotLocal = OverlayInputGeometry.AbsoluteToLocal(
						PanelGeometry.LocalToAbsolute(PivotPanel));
					const FVector2f CursorLocal = OverlayInputGeometry.AbsoluteToLocal(CursorScreen);
					const EBlendViewTransformMode SharedMode =
						CursorMode == EBlendViewGraphTransformMode::Rotate
							? EBlendViewTransformMode::Rotate
							: EBlendViewTransformMode::Scale;
					MaxLayerId = FMath::Max(
						MaxLayerId,
						BlendViewTransformCursorRenderer::PaintSoftwareCursor(
							AllottedGeometry,
							OutDrawElements,
							LayerId + 20,
							SharedMode,
							FVector2D(PivotLocal),
							FVector2D(CursorLocal)));
				}
			}
			return MaxLayerId;
		}

		FVector2f StartGraph = FVector2f::ZeroVector;
		FVector2f EndScreen = FVector2f::ZeroVector;
		FVector2f CursorPivotGraph = FVector2f::ZeroVector;
		FVector2f CursorScreen = FVector2f::ZeroVector;
		TWeakPtr<SWidget> ValueHost;
		TWeakPtr<SGraphPanel> TransformPanel;
		FText ValueText;
		EBlendViewGraphTransformMode CursorMode = EBlendViewGraphTransformMode::Translate;
		bool bVisible = false;
		bool bUsingEditorFallback = false;
		bool bCursorVisible = false;
	};

	bool MatchesGraphCommand(
		const FBlendViewInputEvent& Event,
		const TSharedPtr<FUICommandInfo>& Command)
	{
		return Command.IsValid() && FBlendViewCommands::MatchesInputEvent(Event, *Command);
	}
}

TOptional<EMouseCursor::Type>
FBlendViewGraphTransformController::GetHardwareCursorOverride() const
{
	return bActive && Mode == EBlendViewGraphTransformMode::Translate
		? TOptional<EMouseCursor::Type>(EMouseCursor::CardinalCross)
		: TOptional<EMouseCursor::Type>();
}

bool FBlendViewGraphTransformController::TryBegin(const FBlendViewInputEvent& Event)
{
	if (bActive || !FBlendViewGraphCommands::IsRegistered())
	{
		return false;
	}

	const FBlendViewGraphCommands& Commands = FBlendViewGraphCommands::Get();
	if (MatchesGraphCommand(Event, Commands.BeginTranslate))
	{
		return Begin(EBlendViewGraphTransformMode::Translate);
	}
	if (MatchesGraphCommand(Event, Commands.BeginRotate))
	{
		return Begin(EBlendViewGraphTransformMode::Rotate);
	}
	if (MatchesGraphCommand(Event, Commands.BeginScale))
	{
		return Begin(EBlendViewGraphTransformMode::Scale);
	}
	return false;
}

bool FBlendViewGraphTransformController::TryDuplicateAndBegin(const FBlendViewInputEvent& Event)
{
	if (bActive ||
		!FBlendViewGraphCommands::IsRegistered() ||
		!MatchesGraphCommand(Event, FBlendViewGraphCommands::Get().DuplicateAndTranslate))
	{
		return false;
	}

	const FBlendViewGraphContext GraphContext =
		FBlendViewGraphContextResolver::ResolveUnderCursor();
	const TSharedPtr<SGraphEditor> GraphEditor = GraphContext.Editor;
	const TSharedPtr<SGraphPanel> Panel = GraphContext.Panel;
	UEdGraph* Graph = GraphEditor.IsValid() ? GraphEditor->GetCurrentGraph() : nullptr;
	if (!GraphEditor.IsValid() || !Panel.IsValid() || !Panel->IsGraphEditable() || !Graph)
	{
		return false;
	}

	const FGraphPanelSelectionSet SelectedObjects = GraphEditor->GetSelectedNodes();
	TSet<UObject*> NodesToExport;
	TArray<TWeakObjectPtr<UEdGraphNode>> OriginalSelection;
	for (UObject* Object : SelectedObjects)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object); IsValid(Node))
		{
			OriginalSelection.Add(Node);
			if (Node->CanDuplicateNode())
			{
				Node->PrepareForCopying();
				NodesToExport.Add(Node);
			}
		}
	}
	if (NodesToExport.IsEmpty())
	{
		return false;
	}

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(NodesToExport, ExportedText);
	RestoreMaterialNodesAfterCopy(NodesToExport);
	if (ExportedText.IsEmpty() || !FEdGraphUtilities::CanImportNodesFromText(Graph, ExportedText))
	{
		return false;
	}

	TUniquePtr<FScopedTransaction> DuplicateTransaction = MakeUnique<FScopedTransaction>(
		NSLOCTEXT("BlendView", "GraphDuplicateAndMoveNodes", "BlendView Duplicate and Move Graph Nodes"));
	Graph->Modify();
	GraphEditor->ClearSelectionSet();

	TSet<UEdGraphNode*> ImportedNodeSet;
	FEdGraphUtilities::ImportNodesFromText(Graph, ExportedText, ImportedNodeSet);
	if (ImportedNodeSet.IsEmpty())
	{
		DuplicateTransaction->Cancel();
		RestoreGraphSelection(GraphEditor, OriginalSelection);
		return false;
	}

	FVector2f AveragePosition = FVector2f::ZeroVector;
	for (UEdGraphNode* Node : ImportedNodeSet)
	{
		AveragePosition += FVector2f(Node->NodePosX, Node->NodePosY);
	}
	AveragePosition /= static_cast<float>(ImportedNodeSet.Num());

	const FVector2f PasteLocation = GraphEditor->GetPasteLocation2f();
	TArray<UEdGraphNode*> ImportedNodes;
	TArray<TWeakObjectPtr<UEdGraphNode>> DuplicateNodesToDeleteOnCancel;
	ImportedNodes.Reserve(ImportedNodeSet.Num());
	DuplicateNodesToDeleteOnCancel.Reserve(ImportedNodeSet.Num());
	for (UEdGraphNode* Node : ImportedNodeSet)
	{
		if (!IsValid(Node))
		{
			continue;
		}

		Node->Modify();
		Node->NodePosX = static_cast<int32>((static_cast<float>(Node->NodePosX) - AveragePosition.X) + PasteLocation.X);
		Node->NodePosY = static_cast<int32>((static_cast<float>(Node->NodePosY) - AveragePosition.Y) + PasteLocation.Y);
		Node->SnapToGrid(SNodePanel::GetSnapGridSize());
		Node->CreateNewGuid();
		GraphEditor->SetNodeSelection(Node, true);
		ImportedNodes.Add(Node);
		DuplicateNodesToDeleteOnCancel.Add(Node);
	}
	Graph->NotifyGraphChanged();

	return BeginWithNodes(
		EBlendViewGraphTransformMode::Translate,
		Panel,
		Graph,
		ImportedNodes,
		MoveTemp(DuplicateNodesToDeleteOnCancel),
		MoveTemp(OriginalSelection),
		MoveTemp(DuplicateTransaction));
}

EBlendViewInputResult FBlendViewGraphTransformController::RouteInput(const FBlendViewInputEvent& Event)
{
	if (!bActive)
	{
		return EBlendViewInputResult::PassThrough;
	}

	if (Event.Type == EBlendViewInputEventType::MouseMove)
	{
		LastPointerScreenPosition = FVector2f(Event.ScreenPosition);
		if (NumericInput.IsActive())
		{
			return EBlendViewInputResult::Handled;
		}

		bPrecisionMode = Event.bShiftDown;
		const FVector2f CurrentPointerGraphPosition = GetPointerGraphPositionFromScreen(Event.ScreenPosition);
		const FVector2f GraphDelta = CurrentPointerGraphPosition - LastPointerGraphPosition;
		const float ActivePrecisionFactor = Mode == EBlendViewGraphTransformMode::Rotate
			? RotationPrecisionFactor
			: PrecisionFactor;
		AccumulatedPointerGraphDelta += GraphDelta * (bPrecisionMode ? ActivePrecisionFactor : 1.0f);
		LastPointerGraphPosition = CurrentPointerGraphPosition;
		FreeMoveIntent = AccumulatedPointerGraphDelta;
		if (bMiddleMouseConstraintSelection)
		{
			UpdateAutomaticConstraint();
		}
		ApplyCurrentIntent();
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::MouseDown && Event.Key == EKeys::MiddleMouseButton)
	{
		if (Mode != EBlendViewGraphTransformMode::Rotate)
		{
			bMiddleMouseConstraintSelection = true;
			UpdateAutomaticConstraint();
			ApplyCurrentIntent();
		}
		return EBlendViewInputResult::Handled;
	}
	if (Event.Type == EBlendViewInputEventType::MouseUp && Event.Key == EKeys::MiddleMouseButton)
	{
		bMiddleMouseConstraintSelection = false;
		return EBlendViewInputResult::Handled;
	}

	if (Event.Type == EBlendViewInputEventType::KeyDown)
	{
		if (Event.bIsRepeat)
		{
			return EBlendViewInputResult::Handled;
		}

		if (FBlendViewCommands::IsRegistered() &&
			MatchesGraphCommand(Event, FBlendViewCommands::Get().ToggleTransformStatusBar))
		{
			FBlendViewSessionState::ToggleStatusHints();
			UpdateStatusBar();
			return EBlendViewInputResult::Handled;
		}

		if (!Event.bControlDown &&
			!Event.bAltDown &&
			!Event.bCommandDown &&
			NumericInput.HandleKey(Event.Key))
		{
			bIncrementSnapActive = false;
			ApplyNumericTransform();
			return EBlendViewInputResult::Handled;
		}
		if (Event.Key == EKeys::BackSpace)
		{
			HandleNumericBackspace();
			return EBlendViewInputResult::Handled;
		}

		if (Event.Key == EKeys::LeftShift || Event.Key == EKeys::RightShift)
		{
			bPrecisionMode = true;
			ApplyCurrentIntent();
			return EBlendViewInputResult::Handled;
		}
		if (IsGraphControlModifierKey(Event.Key))
		{
			bIncrementSnapActive = !NumericInput.IsActive();
			ApplyCurrentIntent();
			return EBlendViewInputResult::Handled;
		}

		const FBlendViewGraphCommands& Commands = FBlendViewGraphCommands::Get();
		if (MatchesGraphCommand(Event, Commands.BeginTranslate))
		{
			SetMode(EBlendViewGraphTransformMode::Translate);
			return EBlendViewInputResult::Handled;
		}
		if (MatchesGraphCommand(Event, Commands.BeginRotate))
		{
			SetMode(EBlendViewGraphTransformMode::Rotate);
			return EBlendViewInputResult::Handled;
		}
		if (MatchesGraphCommand(Event, Commands.BeginScale))
		{
			SetMode(EBlendViewGraphTransformMode::Scale);
			return EBlendViewInputResult::Handled;
		}
		if (Event.Key == EKeys::X)
		{
			ToggleConstraint(EBlendViewGraphConstraint::X);
			return EBlendViewInputResult::Handled;
		}
		if (Event.Key == EKeys::Y)
		{
			ToggleConstraint(EBlendViewGraphConstraint::Y);
			return EBlendViewInputResult::Handled;
		}
		if (FBlendViewCommands::IsRegistered() &&
			MatchesGraphCommand(Event, FBlendViewCommands::Get().ClearTransformConstraint))
		{
			ClearConstraint();
			return EBlendViewInputResult::Handled;
		}
	}
	else if (Event.Type == EBlendViewInputEventType::KeyUp &&
		(Event.Key == EKeys::LeftShift || Event.Key == EKeys::RightShift))
	{
		bPrecisionMode = Event.bShiftDown;
		ApplyCurrentIntent();
		return EBlendViewInputResult::Handled;
	}
	else if (Event.Type == EBlendViewInputEventType::KeyUp && IsGraphControlModifierKey(Event.Key))
	{
		bIncrementSnapActive = false;
		ApplyCurrentIntent();
		return EBlendViewInputResult::Handled;
	}

	return EBlendViewInputResult::Handled;
}

void FBlendViewGraphTransformController::Confirm()
{
	if (!bActive)
	{
		return;
	}

	FinalizeNodeInteraction();
	MarkGraphOwnerModified();
	NodesToDeleteOnCancel.Reset();
	SelectionToRestoreOnCancel.Reset();
	ActiveTransaction.Reset();
	Finish(true);
}

void FBlendViewGraphTransformController::Cancel()
{
	if (!bActive)
	{
		return;
	}

	RestoreOriginalPositions();
	FinalizeNodeInteraction();
	if (ActiveTransaction.IsValid())
	{
		ActiveTransaction->Cancel();
		ActiveTransaction.Reset();
	}
	DeleteDuplicatedNodesOnCancel();
	RestoreSelectionAfterDuplicateCancel();
	Finish(true);
}

bool FBlendViewGraphTransformController::Begin(const EBlendViewGraphTransformMode RequestedMode)
{
	const TSharedPtr<SGraphPanel> Panel =
		FBlendViewGraphContextResolver::ResolveUnderCursor().Panel;
	if (!Panel.IsValid() || !Panel->IsGraphEditable())
	{
		return false;
	}

	UEdGraph* Graph = Panel->GetGraphObj();
	const TArray<UEdGraphNode*> SelectedNodes = Panel->GetSelectedGraphNodes();
	if (!Graph || SelectedNodes.IsEmpty())
	{
		return false;
	}

	return BeginWithNodes(
		RequestedMode,
		Panel,
		Graph,
		SelectedNodes,
		TArray<TWeakObjectPtr<UEdGraphNode>>());
}

bool FBlendViewGraphTransformController::BeginWithNodes(
	const EBlendViewGraphTransformMode RequestedMode,
	const TSharedPtr<SGraphPanel>& Panel,
	UEdGraph* Graph,
	const TArray<UEdGraphNode*>& NodesToTransform,
	TArray<TWeakObjectPtr<UEdGraphNode>>&& InNodesToDeleteOnCancel,
	TArray<TWeakObjectPtr<UEdGraphNode>>&& InSelectionToRestoreOnCancel,
	TUniquePtr<FScopedTransaction> InTransaction)
{
	if (!Panel.IsValid() || !Panel->IsGraphEditable() || !Graph || NodesToTransform.IsEmpty())
	{
		if (InTransaction.IsValid())
		{
			InTransaction->Cancel();
		}
		TArray<UEdGraphNode*> NodesToDelete;
		for (const TWeakObjectPtr<UEdGraphNode>& WeakNode : InNodesToDeleteOnCancel)
		{
			if (UEdGraphNode* Node = WeakNode.Get())
			{
				NodesToDelete.Add(Node);
			}
		}
		DeleteGraphNodes(Graph, NodesToDelete);
		RestoreGraphSelection(
			FBlendViewGraphContextResolver::ResolveUnderCursor().Editor,
			InSelectionToRestoreOnCancel);
		return false;
	}

	TSet<UEdGraphNode*> TransformNodeSet;
	for (UEdGraphNode* Node : NodesToTransform)
	{
		if (!IsValid(Node))
		{
			continue;
		}

		TransformNodeSet.Add(Node);
		if (const UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node);
			CommentNode && CommentNode->MoveMode == ECommentBoxMode::GroupMovement)
		{
			for (UObject* ObjectUnderComment : CommentNode->GetNodesUnderComment())
			{
				if (UEdGraphNode* NodeUnderComment = Cast<UEdGraphNode>(ObjectUnderComment);
					IsValid(NodeUnderComment))
				{
					TransformNodeSet.Add(NodeUnderComment);
				}
			}
		}
	}

	Nodes.Reset();
	TArray<FVector2f> NodePositions;
	NodePositions.Reserve(TransformNodeSet.Num());
	for (UEdGraphNode* Node : TransformNodeSet)
	{
		if (!IsValid(Node))
		{
			continue;
		}
		FNodeStartState& State = Nodes.AddDefaulted_GetRef();
		State.Node = Node;
		State.Position = FVector2f(Node->GetNodePosX(), Node->GetNodePosY());
		NodePositions.Add(State.Position);
	}
	if (Nodes.IsEmpty())
	{
		if (InTransaction.IsValid())
		{
			InTransaction->Cancel();
		}
		TArray<UEdGraphNode*> NodesToDelete;
		for (const TWeakObjectPtr<UEdGraphNode>& WeakNode : InNodesToDeleteOnCancel)
		{
			if (UEdGraphNode* Node = WeakNode.Get())
			{
				NodesToDelete.Add(Node);
			}
		}
		DeleteGraphNodes(Graph, NodesToDelete);
		RestoreGraphSelection(
			FBlendViewGraphContextResolver::ResolveUnderCursor().Editor,
			InSelectionToRestoreOnCancel);
		return false;
	}

	ActivePanel = Panel;
	ActiveGraphEditor = FBlendViewGraphContextResolver::ResolveUnderCursor().Editor;
	ActiveGraph = Graph;
	Pivot = FBlendViewGraphTransformMath::CalculatePivot(NodePositions);
	Mode = RequestedMode;
	Constraint = EBlendViewGraphConstraint::None;
	bPrecisionMode = false;
	bIncrementSnapActive = false;
	bMiddleMouseConstraintSelection = false;
	NumericInput.Clear();
	FreeMoveIntent = FVector2f::ZeroVector;
	NodesToDeleteOnCancel = MoveTemp(InNodesToDeleteOnCancel);
	SelectionToRestoreOnCancel = MoveTemp(InSelectionToRestoreOnCancel);
	bDuplicateOperation = !NodesToDeleteOnCancel.IsEmpty();
	bDuplicateNeedsStructuralModification = false;
	if (bDuplicateOperation)
	{
		for (const FNodeStartState& State : Nodes)
		{
			if (const UK2Node* K2Node = Cast<UK2Node>(State.Node.Get()))
			{
				bDuplicateNeedsStructuralModification |= K2Node->NodeCausesStructuralBlueprintChange();
			}
		}
	}
	ActiveTransaction = MoveTemp(InTransaction);
	if (!ActiveTransaction.IsValid())
	{
		ActiveTransaction = MakeUnique<FScopedTransaction>(
			NSLOCTEXT("BlendView", "GraphTransformNodes", "BlendView Transform Graph Nodes"));
	}
	for (const FNodeStartState& State : Nodes)
	{
		if (UEdGraphNode* Node = State.Node.Get())
		{
			Node->Modify();
		}
	}
	ResetInputBaseline();
	bActive = true;
	AttachGuideOverlay();
	UpdateCursorOverride();
	UpdateGuideOverlay();
	UpdateStatusBar();
	return true;
}

void FBlendViewGraphTransformController::SetMode(const EBlendViewGraphTransformMode RequestedMode)
{
	if (Mode == RequestedMode)
	{
		return;
	}

	RestoreOriginalPositions();
	Mode = RequestedMode;
	Constraint = EBlendViewGraphConstraint::None;
	bMiddleMouseConstraintSelection = false;
	NumericInput.Clear();
	FreeMoveIntent = FVector2f::ZeroVector;
	ResetInputBaseline();
	UpdateCursorOverride();
	InvalidatePanel();
	UpdateGuideOverlay();
	UpdateStatusBar();
}

void FBlendViewGraphTransformController::ToggleConstraint(const EBlendViewGraphConstraint RequestedConstraint)
{
	Constraint = Constraint == RequestedConstraint ? EBlendViewGraphConstraint::None : RequestedConstraint;
	if (NumericInput.IsActive())
	{
		ApplyNumericTransform();
	}
	else
	{
		ApplyCurrentIntent();
	}
}

void FBlendViewGraphTransformController::ClearConstraint()
{
	if (Constraint == EBlendViewGraphConstraint::None)
	{
		return;
	}

	Constraint = EBlendViewGraphConstraint::None;
	if (NumericInput.IsActive())
	{
		ApplyNumericTransform();
	}
	else
	{
		ApplyCurrentIntent();
	}
}

void FBlendViewGraphTransformController::UpdateAutomaticConstraint()
{
	if (Mode == EBlendViewGraphTransformMode::Rotate)
	{
		return;
	}

	const FVector2f PointerDirection = FreeMoveIntent;
	if (PointerDirection.IsNearlyZero())
	{
		return;
	}
	Constraint = FMath::Abs(PointerDirection.X) >= FMath::Abs(PointerDirection.Y)
		? EBlendViewGraphConstraint::X
		: EBlendViewGraphConstraint::Y;

	if (NumericInput.IsActive())
	{
		ApplyNumericTransform();
	}
}

void FBlendViewGraphTransformController::ApplyCurrentIntent()
{
	if (NumericInput.IsActive())
	{
		ApplyNumericTransform();
		return;
	}

	const TSharedPtr<SGraphPanel> Panel = ActivePanel.Pin();
	UEdGraph* Graph = ActiveGraph.Get();
	const UEdGraphSchema* Schema = Graph ? Graph->GetSchema() : nullptr;
	if (!Panel.IsValid() || !Schema)
	{
		Cancel();
		return;
	}

	const FVector2f CurrentPointer = GetCurrentPointerGraphPosition();
	const FVector2f TranslationDelta =
		FBlendViewGraphTransformMath::ConstrainTranslation(FreeMoveIntent, Constraint);

	double RotationRadians = 0.0;
	if (Mode == EBlendViewGraphTransformMode::Rotate)
	{
		const FVector2f CurrentVector = CurrentPointer - Pivot;
		RotationAccumulator.Evaluate(
			FVector(CurrentVector.X, CurrentVector.Y, 0.0f),
			-FVector::ZAxisVector,
			RotationRadians);
		if (bIncrementSnapActive)
		{
			const float SnapDegrees = bPrecisionMode ? RotationPrecisionSnapDegrees : RotationSnapDegrees;
			RotationRadians = FMath::DegreesToRadians(
				FMath::GridSnap(FMath::RadiansToDegrees(RotationRadians), SnapDegrees));
		}
		RotationRadians = FMath::DegreesToRadians(
			FBlendViewTransformPrecision::CleanNearInteger(
				FMath::RadiansToDegrees(RotationRadians)));
	}

	double ScaleFactor = 1.0;
	if (Mode == EBlendViewGraphTransformMode::Scale)
	{
		ScaleSpring.Evaluate(FVector2D(CurrentPointer), ScaleFactor);
		if (bIncrementSnapActive)
		{
			const float SnapStep = bPrecisionMode ? ScalePrecisionSnapStep : ScaleSnapStep;
			ScaleFactor = FMath::GridSnap(ScaleFactor, SnapStep);
		}
		ScaleFactor = FBlendViewTransformPrecision::CleanNearInteger(ScaleFactor);
	}

	const float GridSize = static_cast<float>(SNodePanel::GetSnapGridSize()) *
		(bPrecisionMode ? PrecisionFactor : 1.0f);
	AppliedTranslationDelta = FBlendViewTransformPrecision::CleanNearInteger(TranslationDelta);
	if (Mode == EBlendViewGraphTransformMode::Translate &&
		bIncrementSnapActive &&
		GridSize > UE_SMALL_NUMBER)
	{
		AppliedTranslationDelta = FVector2f(
			FMath::GridSnap(TranslationDelta.X, GridSize),
			FMath::GridSnap(TranslationDelta.Y, GridSize));
	}
	AppliedRotationRadians = RotationRadians;
	AppliedScaleFactors = FVector2f(
		Constraint == EBlendViewGraphConstraint::Y ? 1.0f : static_cast<float>(ScaleFactor),
		Constraint == EBlendViewGraphConstraint::X ? 1.0f : static_cast<float>(ScaleFactor));

	for (const FNodeStartState& State : Nodes)
	{
		UEdGraphNode* Node = State.Node.Get();
		if (!Node)
		{
			continue;
		}

		FVector2f NewPosition = State.Position;
		if (Mode == EBlendViewGraphTransformMode::Translate)
		{
			if (bIncrementSnapActive && GridSize > UE_SMALL_NUMBER)
			{
				const FVector2f SnappedInitial(
					FMath::GridSnap(State.Position.X, GridSize),
					FMath::GridSnap(State.Position.Y, GridSize));
				NewPosition = SnappedInitial + AppliedTranslationDelta;
			}
			else
			{
				NewPosition += TranslationDelta;
			}
		}
		else if (Mode == EBlendViewGraphTransformMode::Rotate)
		{
			NewPosition = FBlendViewGraphTransformMath::RotatePosition(
				State.Position,
				Pivot,
				RotationRadians);
		}
		else
		{
			NewPosition = FBlendViewGraphTransformMath::ScalePosition(
				State.Position,
				Pivot,
				ScaleFactor,
				Constraint);
		}

		Schema->SetNodePosition(Node, NewPosition);
	}
	InvalidatePanel();
	UpdateGuideOverlay();
	UpdateStatusBar();
}

void FBlendViewGraphTransformController::ApplyNumericTransform()
{
	if (!NumericInput.IsActive())
	{
		return;
	}

	UEdGraph* Graph = ActiveGraph.Get();
	const UEdGraphSchema* Schema = Graph ? Graph->GetSchema() : nullptr;
	if (!Schema)
	{
		Cancel();
		return;
	}

	const double Value = FBlendViewTransformPrecision::CleanNearInteger(NumericInput.GetValue());
	const FVector2f NumericTranslation =
		FBlendViewTransformPrecision::CleanNearInteger(
			FBlendViewGraphTransformMath::GetNumericTranslationDirection(FreeMoveIntent, Constraint) *
			static_cast<float>(Value));
	const double RotationRadians = FMath::DegreesToRadians(Value);
	AppliedTranslationDelta = NumericTranslation;
	AppliedRotationRadians = RotationRadians;
	AppliedScaleFactors = FVector2f(
		Constraint == EBlendViewGraphConstraint::Y ? 1.0f : static_cast<float>(Value),
		Constraint == EBlendViewGraphConstraint::X ? 1.0f : static_cast<float>(Value));

	for (const FNodeStartState& State : Nodes)
	{
		UEdGraphNode* Node = State.Node.Get();
		if (!Node)
		{
			continue;
		}

		FVector2f NewPosition = State.Position;
		switch (Mode)
		{
		case EBlendViewGraphTransformMode::Translate:
			NewPosition += NumericTranslation;
			break;
		case EBlendViewGraphTransformMode::Rotate:
			NewPosition = FBlendViewGraphTransformMath::RotatePosition(
				State.Position,
				Pivot,
				RotationRadians);
			break;
		case EBlendViewGraphTransformMode::Scale:
			NewPosition = FBlendViewGraphTransformMath::ScalePosition(
				State.Position,
				Pivot,
				Value,
				Constraint);
			break;
		default:
			break;
		}
		Schema->SetNodePosition(Node, NewPosition);
	}

	InvalidatePanel();
	UpdateGuideOverlay();
	UpdateStatusBar();
}

void FBlendViewGraphTransformController::HandleNumericBackspace()
{
	const EBlendViewNumericBackspaceResult Result = NumericInput.HandleBackspace();
	if (Result == EBlendViewNumericBackspaceResult::Ignored)
	{
		return;
	}
	if (Result == EBlendViewNumericBackspaceResult::Cleared)
	{
		RestoreOriginalPositions();
		ResetInputBaseline();
		InvalidatePanel();
		UpdateGuideOverlay();
		UpdateStatusBar();
		return;
	}
	ApplyNumericTransform();
}

void FBlendViewGraphTransformController::ResetInputBaseline()
{
	ResetAppliedTransformValues();
	AccumulatedPointerGraphDelta = FVector2f::ZeroVector;
	LastPointerScreenPosition = FSlateApplication::Get().GetCursorPos();
	StartPointerGraphPosition = GetPointerGraphPositionFromScreen(
		FVector2D(LastPointerScreenPosition));
	LastPointerGraphPosition = StartPointerGraphPosition;
	FreeMoveIntent = FVector2f::ZeroVector;
	RotationAccumulator.Reset();
	ScaleSpring.Reset();

	const FVector2f InitialVector = StartPointerGraphPosition - Pivot;
	if (Mode == EBlendViewGraphTransformMode::Rotate)
	{
		RotationAccumulator.Begin(FVector(InitialVector.X, InitialVector.Y, 0.0f));
	}
	else if (Mode == EBlendViewGraphTransformMode::Scale)
	{
		ScaleSpring.Begin(FVector2D(Pivot), FVector2D(StartPointerGraphPosition));
	}
}

void FBlendViewGraphTransformController::RestoreOriginalPositions()
{
	UEdGraph* Graph = ActiveGraph.Get();
	const UEdGraphSchema* Schema = Graph ? Graph->GetSchema() : nullptr;
	if (!Schema)
	{
		return;
	}

	for (const FNodeStartState& State : Nodes)
	{
		if (UEdGraphNode* Node = State.Node.Get())
		{
			Schema->SetNodePosition(Node, State.Position);
		}
	}
}

void FBlendViewGraphTransformController::DeleteDuplicatedNodesOnCancel()
{
	UEdGraph* Graph = ActiveGraph.Get();
	if (!Graph || NodesToDeleteOnCancel.IsEmpty())
	{
		return;
	}

	TArray<UEdGraphNode*> NodesToDelete;
	NodesToDelete.Reserve(NodesToDeleteOnCancel.Num());
	for (const TWeakObjectPtr<UEdGraphNode>& WeakNode : NodesToDeleteOnCancel)
	{
		if (UEdGraphNode* Node = WeakNode.Get();
			IsValid(Node) && Node->GetGraph() == Graph)
		{
			NodesToDelete.Add(Node);
		}
	}
	NodesToDeleteOnCancel.Reset();
	if (NodesToDelete.IsEmpty())
	{
		return;
	}

	DeleteGraphNodes(Graph, NodesToDelete);
}

void FBlendViewGraphTransformController::RestoreSelectionAfterDuplicateCancel()
{
	const TSharedPtr<SGraphEditor> GraphEditor = ActiveGraphEditor.Pin();
	if (!GraphEditor.IsValid() || SelectionToRestoreOnCancel.IsEmpty())
	{
		SelectionToRestoreOnCancel.Reset();
		return;
	}

	RestoreGraphSelection(GraphEditor, SelectionToRestoreOnCancel);
	SelectionToRestoreOnCancel.Reset();
}

void FBlendViewGraphTransformController::FinalizeNodeInteraction() const
{
	const TSharedPtr<SGraphPanel> Panel = ActivePanel.Pin();
	if (!Panel.IsValid())
	{
		return;
	}

	for (const FNodeStartState& State : Nodes)
	{
		const UEdGraphNode* Node = State.Node.Get();
		if (!Node)
		{
			continue;
		}
		if (const TSharedPtr<SGraphNode> NodeWidget = Panel->GetNodeWidgetFromGuid(Node->NodeGuid))
		{
			NodeWidget->EndUserInteraction();
		}
	}
}

void FBlendViewGraphTransformController::MarkGraphOwnerModified() const
{
	if (!bDuplicateOperation)
	{
		return;
	}

	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ActiveGraph.Get()))
	{
		if (bDuplicateNeedsStructuralModification)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		else
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}
}

void FBlendViewGraphTransformController::Finish(const bool bNotifyGraph)
{
	if (bNotifyGraph)
	{
		if (UEdGraph* Graph = ActiveGraph.Get())
		{
			Graph->NotifyGraphChanged();
		}
	}
	FBlendViewStatusBarPresenter::Get().Restore();
	RestoreCursorOverride();
	DetachGuideOverlay();
	InvalidatePanel();
	ActivePanel.Reset();
	ActiveGraphEditor.Reset();
	ActiveGraph.Reset();
	Nodes.Reset();
	NodesToDeleteOnCancel.Reset();
	SelectionToRestoreOnCancel.Reset();
	ActiveTransaction.Reset();
	AccumulatedPointerGraphDelta = FVector2f::ZeroVector;
	LastPointerGraphPosition = FVector2f::ZeroVector;
	LastPointerScreenPosition = FVector2f::ZeroVector;
	FreeMoveIntent = FVector2f::ZeroVector;
	Constraint = EBlendViewGraphConstraint::None;
	bPrecisionMode = false;
	bIncrementSnapActive = false;
	bMiddleMouseConstraintSelection = false;
	bDuplicateOperation = false;
	bDuplicateNeedsStructuralModification = false;
	RotationAccumulator.Reset();
	ScaleSpring.Reset();
	NumericInput.Clear();
	ResetAppliedTransformValues();
	bActive = false;
}

void FBlendViewGraphTransformController::AttachGuideOverlay()
{
	if (GuideOverlay.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	const TSharedPtr<SGraphPanel> Panel = ActivePanel.Pin();
	if (!Panel.IsValid())
	{
		return;
	}

	TSharedPtr<SBlendViewGraphGuideOverlay> Overlay;
	SAssignNew(Overlay, SBlendViewGraphGuideOverlay)
		.Visibility(EVisibility::HitTestInvisible);
	if (GuideOverlayAttachment.Attach(
		Panel,
		Overlay.ToSharedRef(),
		10000,
		EBlendViewGraphOverlayHost::Window))
	{
		GuideOverlay = Overlay;
	}

	TSharedPtr<SBlendViewGraphGuideOverlay> PanelOverlay;
	SAssignNew(PanelOverlay, SBlendViewGraphGuideOverlay)
		.Visibility(EVisibility::HitTestInvisible);
	if (TransformOverlayAttachment.Attach(
		Panel,
		PanelOverlay.ToSharedRef(),
		10000,
		EBlendViewGraphOverlayHost::Panel))
	{
		TransformOverlay = PanelOverlay;
	}
}

void FBlendViewGraphTransformController::DetachGuideOverlay()
{
	GuideOverlayAttachment.Detach();
	TransformOverlayAttachment.Detach();
	GuideOverlay.Reset();
	TransformOverlay.Reset();
}

void FBlendViewGraphTransformController::UpdateCursorOverride()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	if (bActive && Mode == EBlendViewGraphTransformMode::Translate)
	{
		if (bCursorVisibilityOverridden)
		{
			if (const TSharedPtr<FSlateUser> CursorUser = FSlateApplication::Get().GetCursorUser())
			{
				CursorUser->SetCursorVisibility(bSavedCursorVisible);
			}
			bCursorVisibilityOverridden = false;
		}
		return;
	}

	const bool bShouldHide =
		bActive &&
		Mode != EBlendViewGraphTransformMode::Translate &&
		(TransformOverlay.IsValid() || GuideOverlay.IsValid());
	if (bShouldHide && !bCursorVisibilityOverridden)
	{
		if (const TSharedPtr<FSlateUser> CursorUser = FSlateApplication::Get().GetCursorUser())
		{
			bSavedCursorVisible = CursorUser->IsCursorVisible();
			CursorUser->SetCursorVisibility(false);
			bCursorVisibilityOverridden = true;
		}
	}
}

void FBlendViewGraphTransformController::RestoreCursorOverride()
{
	if (FSlateApplication::IsInitialized())
	{
		if (bCursorVisibilityOverridden)
		{
			if (const TSharedPtr<FSlateUser> CursorUser = FSlateApplication::Get().GetCursorUser())
			{
				CursorUser->SetCursorVisibility(bSavedCursorVisible);
			}
		}
		FSlateApplication::Get().QueryCursor();
	}
	bCursorVisibilityOverridden = false;
}

void FBlendViewGraphTransformController::PrepareForEngineExit()
{
	// UnrealEd may already be partially torn down, so do not end an outstanding transaction here.
	RestoreCursorOverride();
	(void)ActiveTransaction.Release();
	GuideOverlayAttachment.Abandon();
	TransformOverlayAttachment.Abandon();
	GuideOverlay.Reset();
	TransformOverlay.Reset();
	ActivePanel.Reset();
	ActiveGraphEditor.Reset();
	ActiveGraph.Reset();
	Nodes.Reset();
	NodesToDeleteOnCancel.Reset();
	AccumulatedPointerGraphDelta = FVector2f::ZeroVector;
	LastPointerGraphPosition = FVector2f::ZeroVector;
	LastPointerScreenPosition = FVector2f::ZeroVector;
	Constraint = EBlendViewGraphConstraint::None;
	bPrecisionMode = false;
	bMiddleMouseConstraintSelection = false;
	bActive = false;
}

void FBlendViewGraphTransformController::UpdateGuideOverlay()
{
	const TSharedPtr<SBlendViewGraphGuideOverlay> Overlay =
		StaticCastSharedPtr<SBlendViewGraphGuideOverlay>(GuideOverlay);
	if (!Overlay.IsValid())
	{
		return;
	}

	const bool bShowGuide =
		bActive &&
		(Mode == EBlendViewGraphTransformMode::Rotate || Mode == EBlendViewGraphTransformMode::Scale);
	const TSharedPtr<SGraphPanel> Panel = ActivePanel.Pin();
	const TSharedPtr<SBlendViewGraphGuideOverlay> PanelOverlay =
		TransformOverlay.IsValid()
			? StaticCastSharedPtr<SBlendViewGraphGuideOverlay>(TransformOverlay)
			: Overlay;
	PanelOverlay->SetGuide(
		Pivot,
		LastPointerScreenPosition,
		Panel,
		bShowGuide);

	TSharedPtr<SWidget> ValueHost;
	bool bUsingEditorFallback = false;
	if (const TSharedPtr<SGraphEditor> GraphEditor = ActiveGraphEditor.Pin())
	{
		ValueHost = GraphEditor->GetTitleBar();
		if (!ValueHost.IsValid())
		{
			ValueHost = GraphEditor;
			bUsingEditorFallback = true;
		}
	}
	Overlay->SetValue(BuildTransformValueText(), ValueHost, bUsingEditorFallback);
	PanelOverlay->SetCursor(
		Mode,
		Pivot,
		LastPointerScreenPosition,
		Panel,
		bActive && Mode != EBlendViewGraphTransformMode::Translate);
}

void FBlendViewGraphTransformController::UpdateStatusBar() const
{
	if (!bActive || !FBlendViewSessionState::AreStatusHintsVisible())
	{
		FBlendViewStatusBarPresenter::Get().Restore();
		return;
	}

	FBlendViewStatusBarPresenter::Get().Present(BuildStatusLine(), ActivePanel.Pin());
}

FBlendViewStatusLine FBlendViewGraphTransformController::BuildStatusLine() const
{
	auto Chord = [](const TSharedPtr<FUICommandInfo>& Command, const TCHAR* Fallback)
	{
		if (Command.IsValid())
		{
			const FInputChord& InputChord =
				*Command->GetActiveChord(EMultipleKeyBindingIndex::Primary);
			if (InputChord.IsValidChord())
			{
				return InputChord.GetInputText();
			}
		}
		return FText::FromString(Fallback);
	};

	FBlendViewTransformStatusState State;
	switch (Mode)
	{
	case EBlendViewGraphTransformMode::Translate:
		State.Mode = EBlendViewTransformMode::Translate;
		break;
	case EBlendViewGraphTransformMode::Rotate:
		State.Mode = EBlendViewTransformMode::Rotate;
		break;
	case EBlendViewGraphTransformMode::Scale:
		State.Mode = EBlendViewTransformMode::Scale;
		break;
	default:
		break;
	}
	switch (Constraint)
	{
	case EBlendViewGraphConstraint::X:
		State.Constraint = EBlendViewAxisConstraint::X;
		break;
	case EBlendViewGraphConstraint::Y:
		State.Constraint = EBlendViewAxisConstraint::Y;
		break;
	default:
		State.Constraint = EBlendViewAxisConstraint::None;
		break;
	}
	State.bSupportsZAxis = false;
	State.bSupportsPlaneConstraint = false;
	State.bSupportsAutoConstraint = Mode != EBlendViewGraphTransformMode::Rotate;
	State.bSupportsAutoConstraintPlane = false;
	State.bSupportsTrackball = false;
	State.bSupportsSnapBase = false;
	State.bSupportsPivotEdit = false;
	State.bUseResizeLabel = true;
	if (NumericInput.IsActive())
	{
		State.NumericBuffer = NumericInput.GetBuffer();
	}

	const FBlendViewGraphCommands& Commands = FBlendViewGraphCommands::Get();
	FBlendViewTransformStatusShortcuts Shortcuts;
	Shortcuts.Translate = Chord(Commands.BeginTranslate, TEXT("G"));
	Shortcuts.Rotate = Chord(Commands.BeginRotate, TEXT("R"));
	Shortcuts.Scale = Chord(Commands.BeginScale, TEXT("S"));
	Shortcuts.PivotEditMode = FText::FromString(TEXT("O"));
	if (FBlendViewCommands::IsRegistered())
	{
		const FBlendViewCommands& SharedCommands = FBlendViewCommands::Get();
		Shortcuts.ClearConstraint = Chord(SharedCommands.ClearTransformConstraint, TEXT("C"));
		Shortcuts.ToggleHints = Chord(SharedCommands.ToggleTransformStatusBar, TEXT("H"));
	}
	else
	{
		Shortcuts.ClearConstraint = FText::FromString(TEXT("C"));
		Shortcuts.ToggleHints = FText::FromString(TEXT("H"));
	}
	return FBlendViewTransformStatusBuilder::Build(State, Shortcuts);
}

FText FBlendViewGraphTransformController::BuildTransformValueText() const
{
	FBlendViewTransformValueState State;
	switch (Mode)
	{
	case EBlendViewGraphTransformMode::Translate:
		State.Mode = EBlendViewTransformMode::Translate;
		break;
	case EBlendViewGraphTransformMode::Rotate:
		State.Mode = EBlendViewTransformMode::Rotate;
		break;
	case EBlendViewGraphTransformMode::Scale:
		State.Mode = EBlendViewTransformMode::Scale;
		break;
	default:
		break;
	}
	switch (Constraint)
	{
	case EBlendViewGraphConstraint::X:
		State.Constraint = EBlendViewAxisConstraint::X;
		break;
	case EBlendViewGraphConstraint::Y:
		State.Constraint = EBlendViewAxisConstraint::Y;
		break;
	default:
		State.Constraint = EBlendViewAxisConstraint::None;
		break;
	}
	State.Translation = FVector(
		AppliedTranslationDelta.X,
		AppliedTranslationDelta.Y,
		0.0f);
	State.RotationDegrees = FMath::RadiansToDegrees(AppliedRotationRadians);
	State.Scale = FVector(
		AppliedScaleFactors.X,
		AppliedScaleFactors.Y,
		1.0f);
	State.bGraph = true;

	const bool bChinese =
		FBlendViewLocalization::GetResolvedLanguage() == EBlendViewResolvedLanguage::Chinese;
	return FBlendViewTransformValueFormatter::Format(
		State,
		EBlendViewTranslationNumericUnit::Centimeters,
		bChinese);
}

void FBlendViewGraphTransformController::ResetAppliedTransformValues()
{
	AppliedTranslationDelta = FVector2f::ZeroVector;
	AppliedRotationRadians = 0.0;
	AppliedScaleFactors = FVector2f(1.0f, 1.0f);
}

FVector2f FBlendViewGraphTransformController::GetCurrentPointerGraphPosition() const
{
	return StartPointerGraphPosition + AccumulatedPointerGraphDelta;
}

FVector2f FBlendViewGraphTransformController::GetPointerGraphPositionFromScreen(const FVector2D& ScreenPosition) const
{
	const TSharedPtr<SGraphPanel> Panel = ActivePanel.Pin();
	if (!Panel.IsValid())
	{
		return FVector2f::ZeroVector;
	}
	const FVector2f LocalPosition = Panel->GetTickSpaceGeometry().AbsoluteToLocal(FVector2f(ScreenPosition));
	return FVector2f(Panel->PanelCoordToGraphCoord(LocalPosition));
}

void FBlendViewGraphTransformController::InvalidatePanel() const
{
	if (const TSharedPtr<SGraphPanel> Panel = ActivePanel.Pin())
	{
		Panel->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}
}
