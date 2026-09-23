// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewTransformTargetAdapter.h"

#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/TransformProxy.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorViewportClient.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Snap/BlendViewSnapSolver.h"
#include "Tools/BlendViewModelingEditPivotBridge.h"
#include "Tools/BlendViewTransformMath.h"
#include "Tools/BlendViewTransformPrecision.h"
#include "Tools/BlendViewTransformSelectionResolver.h"
#include "UnrealEdGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlendViewTransformTarget, Log, All);

namespace
{
	FVector GetActorEffectivePivotLocation(const AActor& Actor)
	{
		return Actor.GetTransform().TransformPosition(Actor.GetPivotOffset());
	}

	FVector TransformPivotOffsetToWorld(const FTransform& Transform, const FVector& PivotOffset)
	{
		return Transform.TransformPosition(PivotOffset);
	}

	FVector TransformWorldPivotToOffset(const FTransform& Transform, const FVector& WorldPivotLocation)
	{
		return Transform.InverseTransformPosition(WorldPivotLocation);
	}

	void RefreshActorPivotEditing()
	{
		if (GUnrealEd)
		{
			GUnrealEd->UpdatePivotLocationForSelection(true);
			GUnrealEd->NoteSelectionChange();
		}
		else if (GEditor)
		{
			GEditor->NoteSelectionChange();
		}

		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}

		FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	void AddActorBounds(const AActor& Actor, FBox& InOutBounds)
	{
		const FBox ActorBounds = Actor.GetComponentsBoundingBox(true, true);
		if (ActorBounds.IsValid)
		{
			InOutBounds += ActorBounds;
		}
		else
		{
			InOutBounds += Actor.GetActorLocation();
		}
	}

	void AddComponentBounds(const USceneComponent& Component, FBox& InOutBounds)
	{
		const FBox ComponentBounds =
			Component.CalcBounds(Component.GetComponentTransform()).GetBox();
		if (ComponentBounds.IsValid)
		{
			InOutBounds += ComponentBounds;
		}
		else
		{
			InOutBounds += Component.GetComponentLocation();
		}
	}
}

bool FBlendViewTransformTargetAdapter::IsActiveModelingPivotToolAvailable()
{
	FBlendViewModelingEditPivotBridge::FTarget Target;
	return FBlendViewModelingEditPivotBridge::FindActiveTarget(Target);
}

bool FBlendViewTransformTargetAdapter::ResetActiveModelingPivotLocation()
{
	return FBlendViewModelingEditPivotBridge::ResetActiveLocation();
}

bool FBlendViewTransformTargetAdapter::ResetActiveModelingPivotRotation()
{
	return FBlendViewModelingEditPivotBridge::ResetActiveRotation();
}

bool FBlendViewTransformTargetAdapter::Capture(
	const FBlendViewViewportContext& Context,
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation,
	TUniquePtr<FScopedTransaction> InTransaction,
	const TArray<TWeakObjectPtr<USceneComponent>>& ExplicitComponents)
{
	Reset();
	if (!GEditor)
	{
		if (InTransaction.IsValid())
		{
			InTransaction->Cancel();
		}
		return false;
	}

	if (Context.Kind == EBlendViewViewportKind::EditorViewport)
	{
		return CaptureNativeComponents(
			Context,
			OutPivotLocation,
			OutLocalConstraintRotation,
			MoveTemp(InTransaction),
			ExplicitComponents);
	}

	if (Context.Kind == EBlendViewViewportKind::LevelEditor)
	{
		return CaptureLevelActors(
			Context,
			OutPivotLocation,
			OutLocalConstraintRotation,
			MoveTemp(InTransaction));
	}

	if (InTransaction.IsValid())
	{
		InTransaction->Cancel();
	}
	return false;
}

bool FBlendViewTransformTargetAdapter::CaptureModelingPivot(
	const FBlendViewViewportContext& Context,
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation)
{
	CancelTransaction();
	Reset();

	if (Context.Kind != EBlendViewViewportKind::LevelEditor || !Context.ViewportClient)
	{
		return false;
	}

	if (!FBlendViewModelingEditPivotBridge::StartToolIfNeeded())
	{
		return false;
	}

	FBlendViewModelingEditPivotBridge::FTarget Target;
	if (!FBlendViewModelingEditPivotBridge::FindActiveTarget(Target))
	{
		return false;
	}

	return BindModelingPivotTarget(
		Target.TransformProxy,
		Target.TransformGizmo,
		OutPivotLocation,
		OutLocalConstraintRotation);
}

bool FBlendViewTransformTargetAdapter::CaptureActiveModelingPivot(
	const FBlendViewViewportContext& Context,
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation)
{
	CancelTransaction();
	Reset();

	if (Context.Kind != EBlendViewViewportKind::LevelEditor || !Context.ViewportClient)
	{
		return false;
	}

	FBlendViewModelingEditPivotBridge::FTarget Target;
	if (!FBlendViewModelingEditPivotBridge::FindActiveTarget(Target))
	{
		return false;
	}

	return BindModelingPivotTarget(
		Target.TransformProxy,
		Target.TransformGizmo,
		OutPivotLocation,
		OutLocalConstraintRotation);
}

bool FBlendViewTransformTargetAdapter::BindModelingPivotTarget(
	UTransformProxy* TransformProxy,
	UCombinedTransformGizmo* TransformGizmo,
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation)
{
	if (!TransformProxy || !TransformGizmo)
	{
		return false;
	}

	TransformProxy->bSetPivotMode = true;
	TransformProxy->BeginPivotEditSequence();
	bModelingPivotEditSequenceActive = true;
	bModelingPivotTransformActive = true;
	ModelingPivotProxy = TransformProxy;
	ModelingPivotGizmo = TransformGizmo;
	InitialModelingPivotTransform = TransformProxy->GetTransform();
	BaselineModelingPivotTransform = InitialModelingPivotTransform;
	FBlendViewModelingEditPivotBridge::CacheInitialTransform(TransformProxy);
	Kind = EBlendViewTransformTargetKind::ModelingPivot;
	OutPivotLocation = InitialModelingPivotTransform.GetLocation();
	OutLocalConstraintRotation = InitialModelingPivotTransform.GetRotation().GetNormalized();

	UE_LOG(LogBlendViewTransformTarget, Verbose, TEXT("Captured Modeling Edit Pivot proxy: %s"),
		*OutPivotLocation.ToString());
	return true;
}

bool FBlendViewTransformTargetAdapter::IsModelingPivotToolStillActive() const
{
	if (!bModelingPivotTransformActive)
	{
		return false;
	}

	UTransformProxy* CurrentProxy = FBlendViewModelingEditPivotBridge::FindActiveTransformProxy();
	return CurrentProxy && CurrentProxy == ModelingPivotProxy.Get() && ModelingPivotGizmo.IsValid();
}

bool FBlendViewTransformTargetAdapter::SetModelingPivotLocation(
	const FVector& Location,
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation)
{
	UTransformProxy* TransformProxy = ModelingPivotProxy.Get();
	UCombinedTransformGizmo* TransformGizmo = ModelingPivotGizmo.Get();
	if (!bModelingPivotTransformActive || !TransformProxy || !TransformGizmo || Location.ContainsNaN())
	{
		return false;
	}

	FTransform NewTransform = TransformProxy->GetTransform();
	NewTransform.SetLocation(FBlendViewTransformPrecision::CleanNearInteger(Location));
	if (NewTransform.ContainsNaN())
	{
		return false;
	}

	TransformProxy->bSetPivotMode = true;
	TransformGizmo->SetNewGizmoTransform(NewTransform);
	InitialModelingPivotTransform = NewTransform;
	BaselineModelingPivotTransform = NewTransform;
	FBlendViewModelingEditPivotBridge::CacheInitialTransform(TransformProxy);
	OutPivotLocation = NewTransform.GetLocation();
	OutLocalConstraintRotation = NewTransform.GetRotation().GetNormalized();
	return true;
}

bool FBlendViewTransformTargetAdapter::ResetModelingPivotLocation(
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation)
{
	UTransformProxy* TransformProxy = ModelingPivotProxy.Get();
	UCombinedTransformGizmo* TransformGizmo = ModelingPivotGizmo.Get();
	if (!bModelingPivotTransformActive || !TransformProxy || !TransformGizmo)
	{
		return false;
	}

	FTransform NewTransform = TransformProxy->GetTransform();
	NewTransform.SetLocation(InitialModelingPivotTransform.GetLocation());
	if (NewTransform.ContainsNaN())
	{
		return false;
	}

	TransformProxy->bSetPivotMode = true;
	TransformGizmo->SetNewGizmoTransform(NewTransform);
	BaselineModelingPivotTransform = NewTransform;
	OutPivotLocation = NewTransform.GetLocation();
	OutLocalConstraintRotation = NewTransform.GetRotation().GetNormalized();
	return true;
}

bool FBlendViewTransformTargetAdapter::ResetModelingPivotRotation(
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation)
{
	UTransformProxy* TransformProxy = ModelingPivotProxy.Get();
	UCombinedTransformGizmo* TransformGizmo = ModelingPivotGizmo.Get();
	if (!bModelingPivotTransformActive || !TransformProxy || !TransformGizmo)
	{
		return false;
	}

	FTransform NewTransform = TransformProxy->GetTransform();
	NewTransform.SetRotation(InitialModelingPivotTransform.GetRotation().GetNormalized());
	if (NewTransform.ContainsNaN())
	{
		return false;
	}

	TransformProxy->bSetPivotMode = true;
	TransformGizmo->SetNewGizmoTransform(NewTransform);
	BaselineModelingPivotTransform = NewTransform;
	OutPivotLocation = NewTransform.GetLocation();
	OutLocalConstraintRotation = NewTransform.GetRotation().GetNormalized();
	return true;
}

bool FBlendViewTransformTargetAdapter::CaptureLevelActors(
	const FBlendViewViewportContext& Context,
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation,
	TUniquePtr<FScopedTransaction> InTransaction)
{
	if (!Context.ViewportClient)
	{
		if (InTransaction.IsValid())
		{
			InTransaction->Cancel();
		}
		return false;
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors)
	{
		if (InTransaction.IsValid())
		{
			InTransaction->Cancel();
		}
		return false;
	}

	TArray<AActor*> Actors;
	SelectedActors->GetSelectedObjects<AActor>(Actors);
	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor) || Actor->IsTemplate())
		{
			continue;
		}

		FBlendViewActorTransformSnapshot Snapshot;
		Snapshot.Actor = Actor;
		Snapshot.InitialTransform = Actor->GetActorTransform();
		Snapshot.BaselineTransform = Snapshot.InitialTransform;
		Snapshot.InitialPivotOffset = Actor->GetPivotOffset();
		Snapshot.BaselinePivotOffset = Snapshot.InitialPivotOffset;
		ActorSnapshots.Add(Snapshot);
	}

	if (ActorSnapshots.IsEmpty())
	{
		if (InTransaction.IsValid())
		{
			InTransaction->Cancel();
		}
		return false;
	}

	ActiveTransaction = MoveTemp(InTransaction);
	if (!ActiveTransaction.IsValid())
	{
		ActiveTransaction = MakeUnique<FScopedTransaction>(
			NSLOCTEXT("BlendView", "TransformActors", "BlendView Transform Actors"));
	}
	FBox CombinedBounds(ForceInit);
	for (const FBlendViewActorTransformSnapshot& Snapshot : ActorSnapshots)
	{
		if (AActor* Actor = Snapshot.Actor.Get())
		{
			Actor->Modify();
			AddActorBounds(*Actor, CombinedBounds);
		}
	}

	Kind = EBlendViewTransformTargetKind::LevelActors;
	const FVector FallbackPivotLocation = ActorSnapshots[0].Actor.IsValid()
		? GetActorEffectivePivotLocation(*ActorSnapshots[0].Actor.Get())
		: ActorSnapshots[0].InitialTransform.GetLocation();
	OutPivotLocation = ActorSnapshots.Num() == 1
		? FallbackPivotLocation
		: (CombinedBounds.IsValid ? CombinedBounds.GetCenter() : FallbackPivotLocation);
	OutLocalConstraintRotation = ActorSnapshots[0].InitialTransform.GetRotation().GetNormalized();
	return true;
}

bool FBlendViewTransformTargetAdapter::CaptureNativeComponents(
	const FBlendViewViewportContext& Context,
	FVector& OutPivotLocation,
	FQuat& OutLocalConstraintRotation,
	TUniquePtr<FScopedTransaction> InTransaction,
	const TArray<TWeakObjectPtr<USceneComponent>>& ExplicitComponents)
{
	if (!Context.ViewportClient || !Context.Viewport)
	{
		return false;
	}

	UWorld* ViewportWorld = Context.ViewportClient->GetWorld();
	TArray<USceneComponent*> SelectedComponents;
	if (ExplicitComponents.IsEmpty())
	{
		FBlendViewTransformSelectionResolver::ResolveNativeSceneComponents(
			Context,
			SelectedComponents);
	}
	else
	{
		for (const TWeakObjectPtr<USceneComponent>& Component : ExplicitComponents)
		{
			if (USceneComponent* ResolvedComponent = Component.Get())
			{
				SelectedComponents.AddUnique(ResolvedComponent);
			}
		}
	}
	for (USceneComponent* Component : SelectedComponents)
	{
		if (Component->IsA<UPrimitiveComponent>() || Component->IsA<UChildActorComponent>())
		{
			AddNativeComponentSnapshot(Component, ViewportWorld);
		}
	}

	if (NativeComponentSnapshots.IsEmpty())
	{
		return false;
	}

	ActiveTransaction = MoveTemp(InTransaction);
	if (!ActiveTransaction.IsValid())
	{
		ActiveTransaction = MakeUnique<FScopedTransaction>(
			NSLOCTEXT("BlendView", "TransformComponents", "BlendView Transform Components"));
	}
	FBox CombinedBounds(ForceInit);
	for (const FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		if (USceneComponent* Component = Snapshot.Component.Get())
		{
			Component->Modify();
			AddComponentBounds(*Component, CombinedBounds);
		}
		if (USceneComponent* TemplateComponent = Snapshot.TemplateComponent.Get())
		{
			TemplateComponent->Modify();
		}
	}

	Kind = EBlendViewTransformTargetKind::NativeComponents;
	NativeComponentContext = Context;
	bNativeComponentTransformActive = true;
	const FVector FallbackPivotLocation = NativeComponentSnapshots[0].InitialTransform.GetLocation();
	OutPivotLocation = NativeComponentSnapshots.Num() == 1
		? FallbackPivotLocation
		: (CombinedBounds.IsValid ? CombinedBounds.GetCenter() : FallbackPivotLocation);
	OutLocalConstraintRotation = NativeComponentSnapshots[0].InitialTransform.GetRotation().GetNormalized();
	return true;
}

void FBlendViewTransformTargetAdapter::AddNativeComponentSnapshot(
	USceneComponent* Component,
	UWorld* ViewportWorld)
{
	if (!IsValid(Component) ||
		Component->GetWorld() != ViewportWorld ||
		(!Component->IsA<UPrimitiveComponent>() && !Component->IsA<UChildActorComponent>()))
	{
		return;
	}

	for (const FBlendViewComponentTransformSnapshot& ExistingSnapshot : NativeComponentSnapshots)
	{
		if (ExistingSnapshot.Component.Get() == Component)
		{
			return;
		}
	}

	FBlendViewComponentTransformSnapshot Snapshot;
	Snapshot.Component = Component;
	Snapshot.ComponentName = Component->GetFName();
	Snapshot.InitialTransform = Component->GetComponentTransform();
	Snapshot.BaselineTransform = Snapshot.InitialTransform;
	if (USceneComponent* TemplateComponent = Cast<USceneComponent>(Component->GetArchetype()))
	{
		if (TemplateComponent != Component && TemplateComponent->GetWorld() != ViewportWorld)
		{
			Snapshot.TemplateComponent = TemplateComponent;
			Snapshot.InitialTemplateRelativeTransform = TemplateComponent->GetRelativeTransform();
			Snapshot.BaselineTemplateRelativeTransform = Snapshot.InitialTemplateRelativeTransform;
		}
	}
	NativeComponentSnapshots.Add(Snapshot);
}

void FBlendViewTransformTargetAdapter::RefreshNativeComponentBindings()
{
	if (!bNativeComponentTransformActive ||
		NativeComponentSnapshots.IsEmpty() ||
		!NativeComponentContext.ViewportClient)
	{
		return;
	}

	TArray<USceneComponent*> CurrentComponents;
	FBlendViewTransformSelectionResolver::ResolveNativeSceneComponents(
		NativeComponentContext,
		CurrentComponents);
	if (CurrentComponents.IsEmpty())
	{
		return;
	}

	TSet<USceneComponent*> BoundComponents;
	for (FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		USceneComponent* PreviousComponent = Snapshot.Component.Get();
		USceneComponent* TemplateComponent = Snapshot.TemplateComponent.Get();
		USceneComponent* MatchedComponent = nullptr;

		for (USceneComponent* Candidate : CurrentComponents)
		{
			if (!IsValid(Candidate) || BoundComponents.Contains(Candidate))
			{
				continue;
			}

			const bool bMatchesTemplate =
				TemplateComponent && Candidate->GetArchetype() == TemplateComponent;
			const bool bMatchesStableName =
				Snapshot.ComponentName != NAME_None &&
				Candidate->GetFName() == Snapshot.ComponentName;
			if (Candidate == PreviousComponent || bMatchesTemplate || bMatchesStableName)
			{
				MatchedComponent = Candidate;
				break;
			}
		}

		if (!MatchedComponent &&
			NativeComponentSnapshots.Num() == 1 &&
			CurrentComponents.Num() == 1)
		{
			MatchedComponent = CurrentComponents[0];
		}

		if (!MatchedComponent)
		{
			continue;
		}

		BoundComponents.Add(MatchedComponent);
		if (MatchedComponent != PreviousComponent)
		{
			Snapshot.Component = MatchedComponent;
			Snapshot.ComponentName = MatchedComponent->GetFName();
			MatchedComponent->Modify();
			UE_LOG(
				LogBlendViewTransformTarget,
				Verbose,
				TEXT("Rebound Blueprint preview component: %s"),
				*MatchedComponent->GetPathName());
		}
	}
}

void FBlendViewTransformTargetAdapter::Confirm()
{
	EndModelingPivotTransform();
	ActiveTransaction.Reset();
}

void FBlendViewTransformTargetAdapter::CancelTransaction()
{
	if (ActiveTransaction.IsValid())
	{
		ActiveTransaction->Cancel();
		ActiveTransaction.Reset();
	}
}

void FBlendViewTransformTargetAdapter::End()
{
	EndModelingPivotTransform();
	EndNativeComponentTransform();
	ActiveTransaction.Reset();
	Reset();
}

void FBlendViewTransformTargetAdapter::PrepareForEngineExit()
{
	// UnrealEd may already be partially torn down. Do not call normal restore/end paths here.
	(void)ActiveTransaction.Release();
	NativeComponentContext = FBlendViewViewportContext();
	Kind = EBlendViewTransformTargetKind::None;
	ActorSnapshots.Reset();
	NativeComponentSnapshots.Reset();
	ModelingPivotProxy.Reset();
	ModelingPivotGizmo.Reset();
	bNativeComponentTransformActive = false;
	bManualNativeComponentTransformApplied = false;
	bModelingPivotTransformActive = false;
	bModelingPivotEditSequenceActive = false;
}

void FBlendViewTransformTargetAdapter::Reset()
{
	Kind = EBlendViewTransformTargetKind::None;
	NativeComponentContext = FBlendViewViewportContext();
	ActorSnapshots.Reset();
	NativeComponentSnapshots.Reset();
	ModelingPivotProxy.Reset();
	ModelingPivotGizmo.Reset();
	InitialModelingPivotTransform = FTransform::Identity;
	BaselineModelingPivotTransform = FTransform::Identity;
	bNativeComponentTransformActive = false;
	bManualNativeComponentTransformApplied = false;
	bModelingPivotTransformActive = false;
	bModelingPivotEditSequenceActive = false;
}

bool FBlendViewTransformTargetAdapter::RestoreInitialTransforms(
	FVector& InOutCurrentPivotLocation,
	const FVector& OriginalPivotLocation)
{
	// Restoring the visible target and its writeback baseline must be atomic. In
	// particular, snap-base reselection restores native components before the
	// next translation is evaluated from BaselineTransform.
	ResetBaselineFromOriginal();

	if (bNativeComponentTransformActive)
	{
		if (bManualNativeComponentTransformApplied)
		{
			RestoreNativeComponentSnapshotsToInitial();
			bManualNativeComponentTransformApplied = false;
			InOutCurrentPivotLocation = OriginalPivotLocation;
			return true;
		}
		InOutCurrentPivotLocation = OriginalPivotLocation;
		return true;
	}

	if (bModelingPivotTransformActive)
	{
		if (UTransformProxy* TransformProxy = ModelingPivotProxy.Get())
		{
			if (UCombinedTransformGizmo* TransformGizmo = ModelingPivotGizmo.Get())
			{
				TransformProxy->bSetPivotMode = true;
				TransformGizmo->SetNewGizmoTransform(InitialModelingPivotTransform);
				BaselineModelingPivotTransform = InitialModelingPivotTransform;
				InOutCurrentPivotLocation = OriginalPivotLocation;
				return true;
			}
		}
		return false;
	}

	if (ActorSnapshots.IsEmpty())
	{
		return false;
	}

	bool bRestoredAny = false;
	for (const FBlendViewActorTransformSnapshot& Snapshot : ActorSnapshots)
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		Actor->SetActorTransform(Snapshot.InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
		Actor->SetPivotOffset(Snapshot.InitialPivotOffset);
		bRestoredAny = true;
	}

	if (bRestoredAny && GEditor)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}
	return true;
}

void FBlendViewTransformTargetAdapter::ResetBaselineFromCurrent()
{
	RefreshNativeComponentBindings();
	for (FBlendViewActorTransformSnapshot& Snapshot : ActorSnapshots)
	{
		if (AActor* Actor = Snapshot.Actor.Get())
		{
			Snapshot.BaselineTransform = Actor->GetActorTransform();
			Snapshot.BaselinePivotOffset = Actor->GetPivotOffset();
		}
	}
	for (FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		if (const USceneComponent* Component = Snapshot.Component.Get())
		{
			Snapshot.BaselineTransform = Component->GetComponentTransform();
		}
		if (const USceneComponent* TemplateComponent = Snapshot.TemplateComponent.Get())
		{
			Snapshot.BaselineTemplateRelativeTransform = TemplateComponent->GetRelativeTransform();
		}
	}
	if (bModelingPivotTransformActive)
	{
		if (UTransformProxy* TransformProxy = ModelingPivotProxy.Get())
		{
			BaselineModelingPivotTransform = TransformProxy->GetTransform();
		}
	}
}

void FBlendViewTransformTargetAdapter::ResetBaselineFromOriginal()
{
	for (FBlendViewActorTransformSnapshot& Snapshot : ActorSnapshots)
	{
		Snapshot.BaselineTransform = Snapshot.InitialTransform;
		Snapshot.BaselinePivotOffset = Snapshot.InitialPivotOffset;
	}
	for (FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		Snapshot.BaselineTransform = Snapshot.InitialTransform;
		Snapshot.BaselineTemplateRelativeTransform = Snapshot.InitialTemplateRelativeTransform;
	}
	if (bModelingPivotTransformActive)
	{
		BaselineModelingPivotTransform = InitialModelingPivotTransform;
	}
}

void FBlendViewTransformTargetAdapter::RestoreNativeComponentSnapshotsToInitial()
{
	RefreshNativeComponentBindings();
	for (const FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		USceneComponent* Component = Snapshot.Component.Get();
		if (!IsValid(Component))
		{
			continue;
		}

		Component->Modify();
		Component->SetWorldTransform(Snapshot.InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
		Component->PostEditComponentMove(false);

		if (USceneComponent* TemplateComponent = Snapshot.TemplateComponent.Get())
		{
			TemplateComponent->Modify();
			TemplateComponent->SetRelativeTransform(Snapshot.InitialTemplateRelativeTransform);
			TemplateComponent->PostEditComponentMove(false);
			TemplateComponent->MarkPackageDirty();
		}
	}

	FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
}

void FBlendViewTransformTargetAdapter::SyncTemplateFromPreview(
	USceneComponent* PreviewComponent,
	USceneComponent* TemplateComponent)
{
	if (!PreviewComponent || !TemplateComponent)
	{
		return;
	}

	TemplateComponent->SetRelativeLocation(PreviewComponent->GetRelativeLocation());
	TemplateComponent->SetRelativeRotationExact(PreviewComponent->GetRelativeRotation());
	TemplateComponent->SetRelativeScale3D(PreviewComponent->GetRelativeScale3D());
	TemplateComponent->MarkPackageDirty();
}

bool FBlendViewTransformTargetAdapter::ApplyNativeComponentSnapshotTransform(
	const FBlendViewComponentTransformSnapshot& Snapshot,
	const FTransform& NewTransform,
	const bool bCleanRotation)
{
	USceneComponent* Component = Snapshot.Component.Get();
	if (!IsValid(Component) || NewTransform.ContainsNaN())
	{
		return false;
	}

	Component->Modify();
	Component->SetWorldTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	if (bCleanRotation)
	{
		Component->SetRelativeRotationExact(
			FBlendViewTransformPrecision::CleanNearInteger(Component->GetRelativeRotation()),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	Component->PostEditComponentMove(false);
	Component->MarkPackageDirty();

	if (USceneComponent* TemplateComponent = Snapshot.TemplateComponent.Get())
	{
		TemplateComponent->Modify();
		SyncTemplateFromPreview(Component, TemplateComponent);
		TemplateComponent->PostEditComponentMove(false);
	}

	return true;
}

void FBlendViewTransformTargetAdapter::EndNativeComponentTransform()
{
	bNativeComponentTransformActive = false;
}

void FBlendViewTransformTargetAdapter::EndModelingPivotTransform()
{
	if (UTransformProxy* TransformProxy = ModelingPivotProxy.Get())
	{
		if (bModelingPivotEditSequenceActive)
		{
			TransformProxy->EndPivotEditSequence();
		}
	}
	bModelingPivotEditSequenceActive = false;
}

bool FBlendViewTransformTargetAdapter::ApplyModelingPivotTranslationDelta(
	const FVector& Delta,
	const FVector& InitialPivotLocation,
	FVector& InOutCurrentPivotLocation)
{
	UTransformProxy* TransformProxy = ModelingPivotProxy.Get();
	UCombinedTransformGizmo* TransformGizmo = ModelingPivotGizmo.Get();
	if (!bModelingPivotTransformActive || !TransformProxy || !TransformGizmo)
	{
		return false;
	}

	FTransform NewTransform = BaselineModelingPivotTransform;
	NewTransform.SetLocation(FBlendViewTransformPrecision::CleanNearInteger(
		BaselineModelingPivotTransform.GetLocation() + Delta));
	if (NewTransform.ContainsNaN())
	{
		return false;
	}

	TransformProxy->bSetPivotMode = true;
	TransformGizmo->SetNewGizmoTransform(NewTransform);
	InOutCurrentPivotLocation = FBlendViewTransformPrecision::CleanNearInteger(
		InitialPivotLocation + Delta);
	return true;
}

bool FBlendViewTransformTargetAdapter::ApplyModelingPivotRotationDelta(
	const FQuat& DeltaRotation,
	const FVector& InitialPivotLocation,
	FVector& InOutCurrentPivotLocation)
{
	UTransformProxy* TransformProxy = ModelingPivotProxy.Get();
	UCombinedTransformGizmo* TransformGizmo = ModelingPivotGizmo.Get();
	if (!bModelingPivotTransformActive || !TransformProxy || !TransformGizmo || !DeltaRotation.IsNormalized())
	{
		return false;
	}

	FTransform NewTransform = BaselineModelingPivotTransform;
	NewTransform.SetRotation((DeltaRotation * BaselineModelingPivotTransform.GetRotation()).GetNormalized());
	if (NewTransform.ContainsNaN())
	{
		return false;
	}

	TransformProxy->bSetPivotMode = true;
	TransformGizmo->SetNewGizmoTransform(NewTransform);
	InOutCurrentPivotLocation = InitialPivotLocation;
	return true;
}

bool FBlendViewTransformTargetAdapter::ApplyNativeComponentTranslationDelta(
	const FVector& Delta,
	const FVector& InitialPivotLocation,
	FVector& InOutCurrentPivotLocation)
{
	RefreshNativeComponentBindings();
	const FVector CleanDelta = FBlendViewSnapSolver::CleanNearIntegerVector(Delta);
	if (NativeComponentSnapshots.IsEmpty())
	{
		return false;
	}

	bool bChangedAny = false;
	for (const FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		FTransform NewTransform = Snapshot.BaselineTransform;
		NewTransform.SetLocation(FBlendViewSnapSolver::CleanNearIntegerVector(
			Snapshot.BaselineTransform.GetLocation() + CleanDelta));
		bChangedAny |= ApplyNativeComponentSnapshotTransform(Snapshot, NewTransform);
	}

	if (!bChangedAny)
	{
		return false;
	}

	bManualNativeComponentTransformApplied = true;
	InOutCurrentPivotLocation = FBlendViewSnapSolver::CleanNearIntegerVector(InitialPivotLocation + CleanDelta);
	if (NativeComponentContext.ViewportClient)
	{
		NativeComponentContext.ViewportClient->Invalidate();
	}
	FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
	return true;
}

bool FBlendViewTransformTargetAdapter::ApplyActorPivotTranslationDelta(
	const FVector& Delta,
	const FVector& InitialPivotLocation,
	FVector& InOutCurrentPivotLocation)
{
	if (Kind != EBlendViewTransformTargetKind::LevelActors || ActorSnapshots.IsEmpty())
	{
		return false;
	}

	const FVector CleanDelta = FBlendViewSnapSolver::CleanNearIntegerVector(Delta);
	bool bChangedAny = false;
	for (const FBlendViewActorTransformSnapshot& Snapshot : ActorSnapshots)
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		const FVector BaselinePivotWorldLocation =
			TransformPivotOffsetToWorld(Snapshot.BaselineTransform, Snapshot.BaselinePivotOffset);
		const FVector NewPivotWorldLocation =
			FBlendViewSnapSolver::CleanNearIntegerVector(BaselinePivotWorldLocation + CleanDelta);
		const FVector NewPivotOffset =
			TransformWorldPivotToOffset(Snapshot.BaselineTransform, NewPivotWorldLocation);

		Actor->Modify();
		Actor->SetPivotOffset(NewPivotOffset);
		Actor->PostEditMove(true);
		Actor->MarkPackageDirty();
		bChangedAny = true;
	}

	if (!bChangedAny)
	{
		return false;
	}

	InOutCurrentPivotLocation = FBlendViewSnapSolver::CleanNearIntegerVector(InitialPivotLocation + CleanDelta);
	RefreshActorPivotEditing();
	return true;
}

bool FBlendViewTransformTargetAdapter::ApplyNativeComponentRotationDelta(
	const FQuat& DeltaRotation,
	const FVector& InitialPivotLocation,
	const bool bUseIndividualOrigins,
	FVector& InOutCurrentPivotLocation)
{
	RefreshNativeComponentBindings();
	if (!DeltaRotation.IsNormalized())
	{
		return false;
	}

	if (NativeComponentSnapshots.IsEmpty())
	{
		return false;
	}

	bool bChangedAny = false;
	for (const FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		const FVector PivotLocation = bUseIndividualOrigins
			? Snapshot.BaselineTransform.GetLocation()
			: InitialPivotLocation;
		FTransform NewTransform = FBlendViewTransformMath::BuildRotatedTransform(
			Snapshot.BaselineTransform,
			PivotLocation,
			DeltaRotation);
		bChangedAny |= ApplyNativeComponentSnapshotTransform(Snapshot, NewTransform, true);
	}

	if (!bChangedAny)
	{
		return false;
	}

	bManualNativeComponentTransformApplied = true;
	InOutCurrentPivotLocation = InitialPivotLocation;
	if (NativeComponentContext.ViewportClient)
	{
		NativeComponentContext.ViewportClient->Invalidate();
	}
	FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
	return true;
}

bool FBlendViewTransformTargetAdapter::ApplyNativeComponentScaleFactor(
	const FVector& ScaleFactor,
	const FVector& InitialPivotLocation,
	const FQuat& ConstraintRotation,
	const bool bLocalConstraint,
	const bool bUseIndividualOrigins,
	FVector& InOutCurrentPivotLocation)
{
	RefreshNativeComponentBindings();
	if (NativeComponentSnapshots.IsEmpty())
	{
		return false;
	}

	bool bChangedAny = false;
	for (const FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		const FVector PivotLocation = bUseIndividualOrigins
			? Snapshot.BaselineTransform.GetLocation()
			: InitialPivotLocation;
		FTransform NewTransform = FBlendViewTransformMath::BuildScaledTransform(
			Snapshot.BaselineTransform,
			PivotLocation,
			ScaleFactor,
			ConstraintRotation,
			bLocalConstraint);
		bChangedAny |= ApplyNativeComponentSnapshotTransform(Snapshot, NewTransform);
	}

	if (!bChangedAny)
	{
		return false;
	}

	bManualNativeComponentTransformApplied = true;
	InOutCurrentPivotLocation = InitialPivotLocation;
	if (NativeComponentContext.ViewportClient)
	{
		NativeComponentContext.ViewportClient->Invalidate();
	}
	FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
	return true;
}

bool FBlendViewTransformTargetAdapter::ApplyNativeComponentMirrorTransform(
	const FVector& MirrorScaleFactor,
	const FVector& InitialPivotLocation,
	const FQuat& ConstraintRotation,
	const bool bLocalConstraint,
	const bool bUseIndividualOrigins,
	FVector& InOutCurrentPivotLocation)
{
	RefreshNativeComponentBindings();
	if (NativeComponentSnapshots.IsEmpty())
	{
		return false;
	}

	bool bChangedAny = false;
	for (const FBlendViewComponentTransformSnapshot& Snapshot : NativeComponentSnapshots)
	{
		const FVector PivotLocation = bUseIndividualOrigins
			? Snapshot.BaselineTransform.GetLocation()
			: InitialPivotLocation;
		const FTransform NewTransform = FBlendViewTransformMath::BuildMirroredTransform(
			Snapshot.BaselineTransform,
			PivotLocation,
			MirrorScaleFactor,
			ConstraintRotation,
			bLocalConstraint);
		bChangedAny |= ApplyNativeComponentSnapshotTransform(Snapshot, NewTransform);
	}

	if (!bChangedAny)
	{
		return false;
	}

	bManualNativeComponentTransformApplied = true;
	InOutCurrentPivotLocation = InitialPivotLocation;
	if (NativeComponentContext.ViewportClient)
	{
		NativeComponentContext.ViewportClient->Invalidate();
	}
	FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
	return true;
}
