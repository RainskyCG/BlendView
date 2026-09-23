// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Viewport/BlendViewViewportContext.h"

class AActor;
class FScopedTransaction;
class USceneComponent;
class UCombinedTransformGizmo;
class UTransformProxy;

struct FBlendViewActorTransformSnapshot
{
	TWeakObjectPtr<AActor> Actor;
	FTransform InitialTransform = FTransform::Identity;
	FTransform BaselineTransform = FTransform::Identity;
	FVector InitialPivotOffset = FVector::ZeroVector;
	FVector BaselinePivotOffset = FVector::ZeroVector;
};

struct FBlendViewComponentTransformSnapshot
{
	TWeakObjectPtr<USceneComponent> Component;
	TWeakObjectPtr<USceneComponent> TemplateComponent;
	FName ComponentName = NAME_None;
	FTransform InitialTransform = FTransform::Identity;
	FTransform BaselineTransform = FTransform::Identity;
	FTransform InitialTemplateRelativeTransform = FTransform::Identity;
	FTransform BaselineTemplateRelativeTransform = FTransform::Identity;
};

enum class EBlendViewTransformTargetKind : uint8
{
	None,
	LevelActors,
	NativeComponents,
	ModelingPivot
};

class FBlendViewTransformTargetAdapter final
{
public:
	static bool IsActiveModelingPivotToolAvailable();
	static bool ResetActiveModelingPivotLocation();
	static bool ResetActiveModelingPivotRotation();

	bool Capture(
		const FBlendViewViewportContext& Context,
		FVector& OutPivotLocation,
		FQuat& OutLocalConstraintRotation,
		TUniquePtr<FScopedTransaction> InTransaction = nullptr,
		const TArray<TWeakObjectPtr<USceneComponent>>& ExplicitComponents = {});
	void Confirm();
	void CancelTransaction();
	void End();
	void PrepareForEngineExit();
	void Reset();
	void RefreshNativeComponentBindings();
	bool CaptureModelingPivot(
		const FBlendViewViewportContext& Context,
		FVector& OutPivotLocation,
		FQuat& OutLocalConstraintRotation);
	bool CaptureActiveModelingPivot(
		const FBlendViewViewportContext& Context,
		FVector& OutPivotLocation,
		FQuat& OutLocalConstraintRotation);
	bool IsModelingPivotToolStillActive() const;
	bool SetModelingPivotLocation(
		const FVector& Location,
		FVector& OutPivotLocation,
		FQuat& OutLocalConstraintRotation);
	bool ResetModelingPivotLocation(FVector& OutPivotLocation, FQuat& OutLocalConstraintRotation);
	bool ResetModelingPivotRotation(FVector& OutPivotLocation, FQuat& OutLocalConstraintRotation);

	bool RestoreInitialTransforms(FVector& InOutCurrentPivotLocation, const FVector& OriginalPivotLocation);
	void ResetBaselineFromCurrent();

	bool ApplyModelingPivotTranslationDelta(const FVector& Delta, const FVector& InitialPivotLocation, FVector& InOutCurrentPivotLocation);
	bool ApplyModelingPivotRotationDelta(const FQuat& DeltaRotation, const FVector& InitialPivotLocation, FVector& InOutCurrentPivotLocation);
	bool ApplyNativeComponentTranslationDelta(const FVector& Delta, const FVector& InitialPivotLocation, FVector& InOutCurrentPivotLocation);
	bool ApplyActorPivotTranslationDelta(const FVector& Delta, const FVector& InitialPivotLocation, FVector& InOutCurrentPivotLocation);
	bool ApplyNativeComponentRotationDelta(
		const FQuat& DeltaRotation,
		const FVector& InitialPivotLocation,
		bool bUseIndividualOrigins,
		FVector& InOutCurrentPivotLocation);
	bool ApplyNativeComponentScaleFactor(
		const FVector& ScaleFactor,
		const FVector& InitialPivotLocation,
		const FQuat& ConstraintRotation,
		bool bLocalConstraint,
		bool bUseIndividualOrigins,
		FVector& InOutCurrentPivotLocation);
	bool ApplyNativeComponentMirrorTransform(
		const FVector& MirrorScaleFactor,
		const FVector& InitialPivotLocation,
		const FQuat& ConstraintRotation,
		bool bLocalConstraint,
		bool bUseIndividualOrigins,
		FVector& InOutCurrentPivotLocation);

	bool IsNativeComponentActive() const { return bNativeComponentTransformActive; }
	bool IsModelingPivotActive() const { return bModelingPivotTransformActive && ModelingPivotProxy.IsValid(); }
	EBlendViewTransformTargetKind GetKind() const { return Kind; }
	const TArray<FBlendViewActorTransformSnapshot>& GetActorSnapshots() const { return ActorSnapshots; }
	TArray<FBlendViewActorTransformSnapshot>& GetMutableActorSnapshots() { return ActorSnapshots; }
	const TArray<FBlendViewComponentTransformSnapshot>& GetComponentSnapshots() const { return NativeComponentSnapshots; }
	TArray<FBlendViewComponentTransformSnapshot>& GetMutableComponentSnapshots() { return NativeComponentSnapshots; }

private:
	bool CaptureLevelActors(
		const FBlendViewViewportContext& Context,
		FVector& OutPivotLocation,
		FQuat& OutLocalConstraintRotation,
		TUniquePtr<FScopedTransaction> InTransaction);
	bool CaptureNativeComponents(
		const FBlendViewViewportContext& Context,
		FVector& OutPivotLocation,
		FQuat& OutLocalConstraintRotation,
		TUniquePtr<FScopedTransaction> InTransaction,
		const TArray<TWeakObjectPtr<USceneComponent>>& ExplicitComponents);
	void AddNativeComponentSnapshot(USceneComponent* Component, UWorld* ViewportWorld);
	bool BindModelingPivotTarget(
		UTransformProxy* TransformProxy,
		UCombinedTransformGizmo* TransformGizmo,
		FVector& OutPivotLocation,
		FQuat& OutLocalConstraintRotation);
	void EndNativeComponentTransform();
	void EndModelingPivotTransform();
	void ResetBaselineFromOriginal();
	void RestoreNativeComponentSnapshotsToInitial();
	void SyncTemplateFromPreview(USceneComponent* PreviewComponent, USceneComponent* TemplateComponent);
	bool ApplyNativeComponentSnapshotTransform(
		const FBlendViewComponentTransformSnapshot& Snapshot,
		const FTransform& NewTransform,
		bool bCleanRotation = false);

	EBlendViewTransformTargetKind Kind = EBlendViewTransformTargetKind::None;
	FBlendViewViewportContext NativeComponentContext;
	TArray<FBlendViewActorTransformSnapshot> ActorSnapshots;
	TArray<FBlendViewComponentTransformSnapshot> NativeComponentSnapshots;
	TWeakObjectPtr<UTransformProxy> ModelingPivotProxy;
	TWeakObjectPtr<UCombinedTransformGizmo> ModelingPivotGizmo;
	FTransform InitialModelingPivotTransform = FTransform::Identity;
	FTransform BaselineModelingPivotTransform = FTransform::Identity;
	TUniquePtr<FScopedTransaction> ActiveTransaction;
	bool bNativeComponentTransformActive = false;
	bool bManualNativeComponentTransformApplied = false;
	bool bModelingPivotTransformActive = false;
	bool bModelingPivotEditSequenceActive = false;
};
