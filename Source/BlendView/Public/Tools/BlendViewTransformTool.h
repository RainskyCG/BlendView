// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewModalTool.h"
#include "Snap/BlendViewSnapSolver.h"
#include "Tools/BlendViewNumericInput.h"
#include "Tools/BlendViewPlaneTranslationSession.h"
#include "Tools/BlendViewPivotEditSession.h"
#include "Tools/BlendViewRotationAccumulator.h"
#include "Tools/BlendViewScaleSpring.h"
#include "Tools/BlendViewTransformTargetAdapter.h"
#include "Tools/BlendViewTransformSnapSession.h"
#include "Tools/BlendViewTransformTypes.h"
#include "Tools/BlendViewVirtualPointerTracker.h"
#include "UI/BlendViewTransformOverlaySession.h"
#include "Viewport/BlendViewViewportContext.h"

class AActor;
class FScopedTransaction;
class SLevelViewport;
class SWidget;
class USceneComponent;

class FBlendViewTransformTool final : public IBlendViewModalTool
{
public:
	explicit FBlendViewTransformTool(EBlendViewTransformMode InMode);
	FBlendViewTransformTool(
		EBlendViewTransformMode InMode,
		TUniquePtr<FScopedTransaction> InTransaction,
		TOptional<FVector> InInitialPivotOverride = TOptional<FVector>(),
		TArray<TWeakObjectPtr<USceneComponent>> InExplicitComponents = {});
	virtual ~FBlendViewTransformTool() override;

	virtual bool Begin(const FBlendViewViewportContext& Context) override;
	virtual bool WantsInputBeforeModalShortcuts(const FBlendViewInputEvent& Event) const override;
	virtual EBlendViewInputResult HandleInput(const FBlendViewInputEvent& Event) override;
	virtual bool ResetCurrentTransform(EBlendViewTransformResetChannel Channel) override;
	virtual void Tick(float DeltaTime) override;
	virtual void DrawHUD(UCanvas* Canvas) override;
	virtual bool IsComplete() const override { return bCompleteRequested; }
	virtual void Confirm() override;
	virtual void Cancel() override;
	virtual void End() override;
	virtual void PrepareForEngineExit() override;

private:
	enum class EScaleSpringResetPolicy : uint8
	{
		Reset,
		Preserve
	};

	bool CaptureSelection(const FBlendViewViewportContext& Context);
	bool UpdateTranslation(const FBlendViewInputEvent& Event);
	bool UpdateRotation(const FBlendViewInputEvent& Event);
	bool UpdateScale(const FBlendViewInputEvent& Event);
	bool UpdateMirror(const FBlendViewInputEvent& Event);
	bool RefreshRotationFromCurrentIntent(bool bIncrementSnapActive);
	bool RefreshScaleFromCurrentIntent(bool bIncrementSnapActive, bool bPrecisionModeActive);
	bool ApplyRotationFromViewportPosition(const FVector2D& ViewportPosition);
	bool ApplyFreeRotationFromViewportDelta(const FVector2D& ViewportDelta);
	bool ApplyScaleFromViewportPosition(const FVector2D& ViewportPosition);
	void ResetScaleSpringInput(const FVector2D& ViewportPosition);
	bool BeginSnapBaseSelection();
	bool UpdateSnapBaseCandidateFromEvent(const FBlendViewInputEvent& Event);
	bool UpdateSnapBaseCandidateFromViewport(const FVector2D& ViewportPosition);
	bool GetSnapBaseCandidateFromViewport(
		const FVector2D& ViewportPosition,
		FVector& OutCandidate,
		EBlendViewSnapTargetKind& OutKind);
	void ConfirmSnapBaseSelection();
	void CancelSnapBaseSelection();
	bool BeginMiddleMouseAxisSelection(const FBlendViewInputEvent& Event);
	bool EndMiddleMouseAxisSelection(const FBlendViewInputEvent& Event);
	bool UpdateMiddleMouseAxisSelection(const FVector2D& ViewportPosition);
	bool GetMiddleMouseGuideViewportSegment(
		const FVector2D& ViewportPosition,
		FVector2D& OutStart,
		FVector2D& OutEnd) const;
	bool GetMiddleMouseGuideOverlaySegment(
		const FVector2D& ViewportPosition,
		FVector2D& OutStart,
		FVector2D& OutEnd) const;
	bool GetMiddleMouseGuideCanvasSegment(
		UCanvas* Canvas,
		const FVector2D& ViewportPosition,
		FVector2D& OutStart,
		FVector2D& OutEnd) const;
	bool RefreshViewportContextAndInputFromCurrentCursor();
	bool AdvanceVirtualMousePosition(const FBlendViewInputEvent& Event, FVector2D& OutViewportPosition);
	bool ProjectWorldToViewport(const FVector& WorldPosition, FVector2D& OutViewportPosition) const;
	bool GetViewportRay(const FVector2D& ViewportPosition, FVector& OutRayStart, FVector& OutRayDirection) const;
	bool GetViewportTraceSegment(
		const FVector2D& ViewportPosition,
		FVector& OutTraceStart,
		FVector& OutTraceEnd) const;
	bool GetMousePlaneIntersection(const FVector2D& ViewportPosition, FVector& OutIntersection) const;
	bool ProjectViewportGuideToAxis(
		EBlendViewAxisConstraint Axis,
		const FVector2D& ViewportPosition,
		double& OutWorldDistance,
		double& OutSign,
		double& OutAlignment) const;
	double GetAxisProjectionWorldDistance() const;
	bool ProjectWorldToCanvas(UCanvas* Canvas, const FVector& WorldPosition, FVector2D& OutCanvasPosition) const;
	bool IsCanvasForInitialViewport(UCanvas* Canvas) const;
	bool ProjectWorldToOverlay(const FVector& WorldPosition, FVector2D& OutNormalizedPosition) const;
	bool ShouldDrawMouseGuide() const;
	void AttachViewportOverlay();
	void DetachViewportOverlay();
	void UpdateViewportOverlay();
	void ApplyTransformCursorOverride();
	void RestoreTransformCursorOverride();
	void UpdateWorldPivotVisualization();
	void ClearWorldPivotVisualization();
	void DrawMouseGuide(UCanvas* Canvas) const;
	void DrawAxisGuides(UCanvas* Canvas) const;
	void DrawAxisGuide(UCanvas* Canvas, EBlendViewAxisConstraint Axis, bool bIsActive) const;
	bool GetAxisCanvasDirection(UCanvas* Canvas, EBlendViewAxisConstraint Axis, double Sign, FVector2D& OutStart, FVector2D& OutDirection) const;
	void ApplyTranslationDelta(const FVector& Delta);
	bool ApplyTranslationDeltaInternal(const FVector& Delta, bool bFromOriginal);
	bool ApplyTranslationDeltaFromOriginal(const FVector& Delta);
	bool RefreshTranslationFromCurrentIntent(bool bSnapActive);
	FVector ApplySnapToTranslationDelta(const FVector& Delta, const FVector& SnapSourceBase, bool bFromOriginal);
	FVector GetTranslationSnapSourceBase(bool bFromOriginal) const;
	bool ShouldAlignRotationToSnapTarget() const;
	void ApplySnapTargetAlignment(FTransform& InOutTransform) const;
	void ApplySnapTargetFilters(FBlendViewSnapQuery& Query, bool bAllowGrid, bool bApplyStructuralLimit) const;
	void ClearActiveSnapState();
	FVector ResolveTranslationSnapSourceBase(
		const FVector& PreferredSnapSourceBase,
		const FVector& Delta,
		bool bFromOriginal,
		const FBlendViewSnapCandidate& SnapTarget) const;
	bool ShouldUseClosestTranslationSnapSource(const FBlendViewSnapCandidate& SnapTarget) const;
	bool TryResolveClosestTranslationSnapSourceBase(
		const FVector& Delta,
		bool bFromOriginal,
		const FVector& SnapTargetLocation,
		FVector& OutSnapSourceBase) const;
	bool GetClosestSelectedVertexSnapSourceBase(
		const FVector& Delta,
		bool bFromOriginal,
		const FVector& SnapTargetLocation,
		FVector& OutSnapSourceBase) const;
	bool GetClosestNativeComponentVertexSnapSourceBase(
		const FVector& Delta,
		bool bFromOriginal,
		const FVector& SnapTargetLocation,
		FVector& OutSnapSourceBase) const;
	FVector ApplyGridSnapToTranslationDelta(const FVector& Delta, double GridSize) const;
	FVector ApplyUnrealTranslationSnapToDelta(const FVector& Delta) const;
	bool ShouldUseTranslationSnap(bool bControlDown) const;
	bool ShouldUseIncrementSnap(bool bControlDown) const;
	double GetTransformSnapRadiusScale() const;
	bool ShouldUseUnrealEditorSnap() const;
	bool ShouldUseUnrealTranslationSnap() const;
	bool ShouldUseUnrealRotationSnap() const;
	bool ShouldUseUnrealScaleSnap() const;
	double SnapRotationAngleRadians(double AngleRadians) const;
	double SnapScaleFactor(double ScaleFactor) const;
	FVector GetScaleFactorVector(double AppliedScaleFactor) const;
	bool ProjectFreeMoveIntentToAxis(EBlendViewAxisConstraint Axis, double PreferredSign, bool bUseLocalConstraint);
	bool BeginPlaneTranslationConstraint(
		EBlendViewAxisConstraint Plane,
		const FVector2D& ViewportPosition,
		bool bUseLocalConstraint);
	bool UpdatePlaneTranslationConstraint(const FVector2D& ViewportPosition);
	bool GetPlanePointerLocation(
		EBlendViewAxisConstraint Plane,
		const FVector2D& ViewportPosition,
		bool bUseLocalConstraint,
		FVector& OutPointerLocation) const;
	int32 GetPlaneExcludedAxisIndex(EBlendViewAxisConstraint Plane) const;
	void ApplyRotationDelta(double AngleRadians);
	void ApplyFreeRotationDelta(const FQuat& DeltaRotation);
	void ApplyScaleFactor(double ScaleFactor);
	void ApplyScaleFactorVector(const FVector& ScaleFactorVector);
	void ApplyMirrorTransform();
	FVector GetMirrorScaleFactorVector() const;
	bool ApplyNativeComponentTranslationDelta(const FVector& Delta);
	bool ApplyModelingPivotTranslationDelta(const FVector& Delta);
	bool ApplyActorPivotTranslationDelta(const FVector& Delta);
	bool ApplyNativeComponentRotationDelta(const FQuat& DeltaRotation);
	bool ApplyModelingPivotRotationDelta(const FQuat& DeltaRotation);
	bool ResetModelingPivotTransformComponent(EBlendViewTransformResetChannel Channel);
	void SetMode(EBlendViewTransformMode InMode);
	void TogglePivotEditMode();
	bool EnterModelingPivotEditMode();
	void EnterFreeRotateMode();
	void ToggleAxisConstraint(EBlendViewAxisConstraint InAxis);
	void TogglePlaneConstraint(EBlendViewAxisConstraint InPlane);
	void ClearConstraint();
	void RestoreOperationBaselineToOriginal();
	void HandleNumericBackspace();
	void ApplyNumericTransform();
	void ResetTransientModalState();
	FVector GetNumericTranslationDirection() const;
	EBlendViewAxisConstraint AxisToPlaneConstraint(EBlendViewAxisConstraint Axis) const;
	bool IsAxisActiveForCurrentConstraint(EBlendViewAxisConstraint Axis) const;
	void ResetInputBaselineForMode(
		EBlendViewTransformMode InMode,
		const FVector2D& ViewportPosition,
		EScaleSpringResetPolicy ScaleSpringPolicy = EScaleSpringResetPolicy::Reset);
	void ResetBaselineFromCurrent(const FVector2D& ViewportPosition);
	FVector ApplyTranslationConstraint(const FVector& Delta) const;
	bool IsSingleAxisConstraint(EBlendViewAxisConstraint Constraint) const;
	bool IsPlaneConstraint(EBlendViewAxisConstraint Constraint) const;
	EBlendViewAxisConstraint GetPlaneExcludedAxis(EBlendViewAxisConstraint Plane) const;
	FVector GetAxisBaseVector(EBlendViewAxisConstraint Axis) const;
	FVector GetConstraintAxisVector() const;
	FQuat GetConstraintSpaceRotation() const;
	bool SetAxisConstraint(EBlendViewAxisConstraint InAxis, double InSign, bool bInLocalConstraint = false);
	FVector GetRotationAxisVector() const;
	FBlendViewStatusLine BuildStatusLine() const;
	FText BuildTransformValueText() const;
	void ResetAppliedTransformValues();
	const TCHAR* GetModeName() const;
	const TCHAR* GetAxisName() const;

	EBlendViewTransformMode Mode;
	EBlendViewAxisConstraint AxisConstraint = EBlendViewAxisConstraint::None;
	double AxisConstraintSign = 1.0;
	bool bLocalConstraint = false;
	FBlendViewViewportContext InitialContext;
	TSharedPtr<SWidget> ViewportLifetimeGuard;
	FQuat LocalConstraintRotation = FQuat::Identity;
	FVector OriginalPivotLocation = FVector::ZeroVector;
	FVector InitialPivotLocation = FVector::ZeroVector;
	FVector CurrentPivotLocation = FVector::ZeroVector;
	FVector OriginalTranslationSnapSourceLocation = FVector::ZeroVector;
	FVector InitialTranslationSnapSourceLocation = FVector::ZeroVector;
	FVector FreeMoveIntentPivotLocation = FVector::ZeroVector;
	FVector BaselineFreeMoveIntentPivotLocation = FVector::ZeroVector;
	FVector InitialMouseWorldLocation = FVector::ZeroVector;
	FVector MouseGuideWorldLocation = FVector::ZeroVector;
	FVector2D InitialMouseViewportPosition = FVector2D::ZeroVector;
	FVector2D LastMouseViewportPosition = FVector2D::ZeroVector;
	FVector2D FreeRotateAccumulatedViewportDelta = FVector2D::ZeroVector;
	FVector2D PivotViewportPosition = FVector2D::ZeroVector;
	FVector AppliedTranslationDelta = FVector::ZeroVector;
	FVector AppliedScaleFactors = FVector::OneVector;
	double AppliedRotationRadians = 0.0;
	TOptional<FText> FrozenTransformValueText;
	FBlendViewTransformTargetAdapter TargetAdapter;
	TUniquePtr<FScopedTransaction> PendingTransaction;
	TArray<TWeakObjectPtr<USceneComponent>> ExplicitComponents;
	TOptional<FVector> InitialPivotOverride;
	FBlendViewSnapSolver SnapSolver;
	FBlendViewNumericInput NumericInput;
	FBlendViewVirtualPointerTracker VirtualPointer;
	FBlendViewPlaneTranslationSession PlaneTranslationSession;
	FBlendViewPivotEditSession PivotEditSession;
	FBlendViewTransformSnapSession SnapSession;
	FBlendViewTransformOverlaySession OverlaySession;
	FBlendViewRotationAccumulator RotationAccumulator;
	FBlendViewScaleSpring ScaleSpring;
	FLinearColor SavedSelectionOutlineColor = FLinearColor::White;
	bool bSavedViewportShowWidget = true;
	bool bViewportWidgetHidden = false;
	bool bSelectionOutlineColorChanged = false;
	bool bHasInitialMouseWorldLocation = false;
	bool bHasMouseGuideWorldLocation = false;
	bool bHasPivotViewportPosition = false;
	bool bMiddleMouseAxisSelection = false;
	bool bMiddleMousePlaneSelection = false;
	bool bIncrementSnapActiveForCurrentTransform = false;
	bool bPrecisionModeActiveForCurrentTransform = false;
	bool bFreeRotateMode = false;
	bool bCompleteRequested = false;
	bool bUseIndividualOrigins = false;
	bool bSavedCursorVisible = true;
	bool bCursorVisibilityOverridden = false;
};
