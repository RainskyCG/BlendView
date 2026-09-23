// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewInputTypes.h"
#include "Graph/BlendViewGraphOverlayAttachment.h"
#include "Graph/BlendViewGraphTransformMath.h"
#include "Tools/BlendViewNumericInput.h"
#include "Tools/BlendViewRotationAccumulator.h"
#include "Tools/BlendViewScaleSpring.h"

class FScopedTransaction;
class SGraphEditor;
class SGraphPanel;
class SWidget;
class UEdGraph;
class UEdGraphNode;
struct FBlendViewStatusLine;

enum class EBlendViewGraphTransformMode : uint8
{
	Translate,
	Rotate,
	Scale
};

class FBlendViewGraphTransformController
{
public:
	bool IsActive() const { return bActive; }
	TOptional<EMouseCursor::Type> GetHardwareCursorOverride() const;
	bool TryBegin(const FBlendViewInputEvent& Event);
	bool TryDuplicateAndBegin(const FBlendViewInputEvent& Event);
	EBlendViewInputResult RouteInput(const FBlendViewInputEvent& Event);
	void Confirm();
	void Cancel();
	void PrepareForEngineExit();

private:
	struct FNodeStartState
	{
		TWeakObjectPtr<UEdGraphNode> Node;
		FVector2f Position = FVector2f::ZeroVector;
	};

	bool Begin(EBlendViewGraphTransformMode RequestedMode);
	bool BeginWithNodes(
		EBlendViewGraphTransformMode RequestedMode,
		const TSharedPtr<SGraphPanel>& Panel,
		UEdGraph* Graph,
		const TArray<UEdGraphNode*>& NodesToTransform,
		TArray<TWeakObjectPtr<UEdGraphNode>>&& InNodesToDeleteOnCancel,
		TArray<TWeakObjectPtr<UEdGraphNode>>&& InSelectionToRestoreOnCancel = {},
		TUniquePtr<FScopedTransaction> InTransaction = nullptr);
	void SetMode(EBlendViewGraphTransformMode RequestedMode);
	void ToggleConstraint(EBlendViewGraphConstraint RequestedConstraint);
	void ClearConstraint();
	void UpdateAutomaticConstraint();
	void ApplyCurrentIntent();
	void ApplyNumericTransform();
	void HandleNumericBackspace();
	void ResetInputBaseline();
	void RestoreOriginalPositions();
	void DeleteDuplicatedNodesOnCancel();
	void RestoreSelectionAfterDuplicateCancel();
	void FinalizeNodeInteraction() const;
	void MarkGraphOwnerModified() const;
	void Finish(bool bNotifyGraph);
	void AttachGuideOverlay();
	void DetachGuideOverlay();
	void UpdateCursorOverride();
	void RestoreCursorOverride();
	void UpdateGuideOverlay();
	void UpdateStatusBar() const;
	FBlendViewStatusLine BuildStatusLine() const;
	FText BuildTransformValueText() const;
	void ResetAppliedTransformValues();
	FVector2f GetCurrentPointerGraphPosition() const;
	FVector2f GetPointerGraphPositionFromScreen(const FVector2D& ScreenPosition) const;
	void InvalidatePanel() const;

	TWeakPtr<SGraphPanel> ActivePanel;
	TWeakPtr<SGraphEditor> ActiveGraphEditor;
	TSharedPtr<SWidget> GuideOverlay;
	TSharedPtr<SWidget> TransformOverlay;
	FBlendViewGraphOverlayAttachment GuideOverlayAttachment;
	FBlendViewGraphOverlayAttachment TransformOverlayAttachment;
	TWeakObjectPtr<UEdGraph> ActiveGraph;
	TArray<FNodeStartState> Nodes;
	TArray<TWeakObjectPtr<UEdGraphNode>> NodesToDeleteOnCancel;
	TArray<TWeakObjectPtr<UEdGraphNode>> SelectionToRestoreOnCancel;
	TUniquePtr<FScopedTransaction> ActiveTransaction;
	FBlendViewNumericInput NumericInput;
	FBlendViewRotationAccumulator RotationAccumulator;
	FBlendViewScaleSpring ScaleSpring;
	FVector2f Pivot = FVector2f::ZeroVector;
	FVector2f StartPointerGraphPosition = FVector2f::ZeroVector;
	FVector2f LastPointerGraphPosition = FVector2f::ZeroVector;
	FVector2f LastPointerScreenPosition = FVector2f::ZeroVector;
	FVector2f AccumulatedPointerGraphDelta = FVector2f::ZeroVector;
	FVector2f FreeMoveIntent = FVector2f::ZeroVector;
	FVector2f AppliedTranslationDelta = FVector2f::ZeroVector;
	FVector2f AppliedScaleFactors = FVector2f(1.0f, 1.0f);
	double AppliedRotationRadians = 0.0;
	EBlendViewGraphTransformMode Mode = EBlendViewGraphTransformMode::Translate;
	EBlendViewGraphConstraint Constraint = EBlendViewGraphConstraint::None;
	bool bPrecisionMode = false;
	bool bIncrementSnapActive = false;
	bool bMiddleMouseConstraintSelection = false;
	bool bDuplicateOperation = false;
	bool bDuplicateNeedsStructuralModification = false;
	bool bCursorVisibilityOverridden = false;
	bool bSavedCursorVisible = true;
	bool bActive = false;
};
