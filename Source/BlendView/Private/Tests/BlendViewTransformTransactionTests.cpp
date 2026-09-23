// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Components/ChildActorComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "LevelEditorViewport.h"
#include "Misc/AutomationTest.h"
#include "Selection.h"
#include "Tools/BlendViewTransformTargetAdapter.h"
#include "Tools/BlendViewTransformTool.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewTransformCancelTransactionTest,
	"BlendView.Transform.Transaction.CancelRestoresInitialTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewTransformCancelTransactionTest::RunTest(const FString& Parameters)
{
	if (!GEditor)
	{
		AddWarning(TEXT("Skipped because GEditor is unavailable."));
		return true;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	FEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (!ViewportClient)
	{
		const TArray<FLevelEditorViewportClient*>& ViewportClients = GEditor->GetLevelViewportClients();
		ViewportClient = ViewportClients.IsEmpty() ? nullptr : ViewportClients[0];
	}
	if (!World || !ViewportClient)
	{
		AddWarning(TEXT("Skipped because an editor world or level viewport is unavailable."));
		return true;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient | RF_Transactional;
	AActor* Actor = World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	if (!Actor)
	{
		AddError(TEXT("Failed to spawn the transaction test actor."));
		return false;
	}

	TArray<TWeakObjectPtr<AActor>> PreviousSelection;
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		TArray<AActor*> SelectedActorObjects;
		SelectedActors->GetSelectedObjects<AActor>(SelectedActorObjects);
		for (AActor* SelectedActor : SelectedActorObjects)
		{
			PreviousSelection.Add(SelectedActor);
		}
	}
	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(Actor, true, false, true);

	const FVector InitialLocation(100.0, 200.0, 300.0);
	const FVector ChangedLocation(450.0, -25.0, 900.0);
	Actor->SetActorLocation(InitialLocation);

	FBlendViewViewportContext Context;
	Context.Kind = EBlendViewViewportKind::LevelEditor;
	Context.ViewportClient = ViewportClient;
	Context.Viewport = ViewportClient->Viewport;

	FBlendViewTransformTargetAdapter Adapter;
	FVector Pivot = FVector::ZeroVector;
	FQuat LocalRotation = FQuat::Identity;
	const bool bCaptured = Adapter.Capture(Context, Pivot, LocalRotation);
	TestTrue(TEXT("Adapter captures the selected actor"), bCaptured);
	if (bCaptured)
	{
		FBlendViewComponentTransformSnapshot ComponentSnapshot;
		ComponentSnapshot.InitialTransform.SetLocation(InitialLocation);
		ComponentSnapshot.BaselineTransform.SetLocation(ChangedLocation);
		Adapter.GetMutableComponentSnapshots().Add(ComponentSnapshot);

		Actor->SetActorLocation(ChangedLocation);
		Adapter.GetMutableActorSnapshots()[0].BaselineTransform.SetLocation(ChangedLocation);
		TestTrue(TEXT("Transform changed during the transaction"), Actor->GetActorLocation().Equals(ChangedLocation));

		Adapter.RestoreInitialTransforms(Pivot, Pivot);
		TestTrue(
			TEXT("Restore resets the transform writeback baseline"),
			Adapter.GetActorSnapshots()[0].BaselineTransform.GetLocation().Equals(InitialLocation));
		TestTrue(
			TEXT("Restore resets the native component writeback baseline"),
			Adapter.GetComponentSnapshots()[0].BaselineTransform.GetLocation().Equals(InitialLocation));
		Adapter.CancelTransaction();
		TestTrue(TEXT("Cancel restores the captured transform"), Actor->GetActorLocation().Equals(InitialLocation));
	}
	Adapter.End();

	GEditor->SelectNone(false, true, false);
	for (const TWeakObjectPtr<AActor>& PreviousActor : PreviousSelection)
	{
		if (PreviousActor.IsValid())
		{
			GEditor->SelectActor(PreviousActor.Get(), true, false, true);
		}
	}
	World->DestroyActor(Actor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewChildActorComponentTransformTest,
	"BlendView.Transform.TargetAdapter.ChildActorComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewChildActorComponentTransformTest::RunTest(const FString& Parameters)
{
	if (!GEditor)
	{
		AddWarning(TEXT("Skipped because GEditor is unavailable."));
		return true;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		AddWarning(TEXT("Skipped because the editor world is unavailable."));
		return true;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient | RF_Transactional;
	AActor* Owner = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	if (!Owner)
	{
		AddError(TEXT("Failed to spawn the Child Actor component test owner."));
		return false;
	}

	UChildActorComponent* ChildActorComponent = NewObject<UChildActorComponent>(
		Owner,
		NAME_None,
		RF_Transient | RF_Transactional);
	Owner->SetRootComponent(ChildActorComponent);
	Owner->AddInstanceComponent(ChildActorComponent);
	ChildActorComponent->RegisterComponent();
	ChildActorComponent->SetWorldTransform(FTransform::Identity);

	FBlendViewTransformTargetAdapter Adapter;
	FBlendViewComponentTransformSnapshot Snapshot;
	Snapshot.Component = ChildActorComponent;
	Snapshot.InitialTransform = ChildActorComponent->GetComponentTransform();
	Snapshot.BaselineTransform = Snapshot.InitialTransform;
	Adapter.GetMutableComponentSnapshots().Add(Snapshot);

	FVector Pivot = FVector::ZeroVector;
	TestTrue(
		TEXT("Child Actor component supports translation writeback"),
		Adapter.ApplyNativeComponentTranslationDelta(FVector(100.0, 200.0, 300.0), FVector::ZeroVector, Pivot));
	TestTrue(
		TEXT("Child Actor component receives the translated world transform"),
		ChildActorComponent->GetComponentLocation().Equals(FVector(100.0, 200.0, 300.0)));

	Adapter.GetMutableComponentSnapshots()[0].BaselineTransform =
		ChildActorComponent->GetComponentTransform();
	const FQuat QuarterTurn(FVector::UpVector, FMath::DegreesToRadians(90.0));
	TestTrue(
		TEXT("Child Actor component supports rotation writeback"),
		Adapter.ApplyNativeComponentRotationDelta(
			QuarterTurn,
			ChildActorComponent->GetComponentLocation(),
			false,
			Pivot));
	TestTrue(
		TEXT("Child Actor component receives the rotation"),
		ChildActorComponent->GetComponentQuat().Equals(QuarterTurn, 0.001));

	Adapter.GetMutableComponentSnapshots()[0].BaselineTransform =
		ChildActorComponent->GetComponentTransform();
	TestTrue(
		TEXT("Child Actor component supports scale writeback"),
		Adapter.ApplyNativeComponentScaleFactor(
			FVector(2.0, 2.0, 2.0),
			ChildActorComponent->GetComponentLocation(),
			FQuat::Identity,
			false,
			false,
			Pivot));
	TestTrue(
		TEXT("Child Actor component receives the scale"),
		ChildActorComponent->GetComponentScale().Equals(FVector(2.0, 2.0, 2.0)));

	Adapter.End();
	World->DestroyActor(Owner);
	return true;
}

#endif
