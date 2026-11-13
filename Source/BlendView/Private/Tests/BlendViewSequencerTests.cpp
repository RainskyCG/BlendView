// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Channels/MovieSceneDoubleChannel.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelEditorViewport.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "ScopedTransaction.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Selection.h"
#include "SequencerSettings.h"
#include "Tools/BlendViewTransformTargetAdapter.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendViewSequencerAutoKeyTest,
	"BlendView.Transform.Sequencer.ConfirmCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewSequencerAutoKeyTest::RunTest(const FString& Parameters)
{
	if (!GEditor || GEditor->GetLevelViewportClients().IsEmpty())
	{
		AddError(TEXT("Sequencer regression requires an editor world and level viewport."));
		return false;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return false;
	}
	TArray<AActor*> Selection;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Selection);
	FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("MovieSceneTools"));
	ISequencerModule& Module = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));

	for (int32 Case = 0; Case < 8; ++Case)
	{
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient | RF_Transactional;
		AActor* Actor = World->SpawnActor<AStaticMeshActor>(Spawn);
		if (!Actor)
		{
			AddError(TEXT("Failed to spawn Sequencer test actor."));
			break;
		}
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Actor, true, false, true);
		AActor* Child = nullptr;
		if (Case == 7)
		{
			Child = World->SpawnActor<AStaticMeshActor>(Spawn);
			if (Child)
			{
				Child->AttachToActor(Actor, FAttachmentTransformRules::KeepRelativeTransform);
				Child->SetActorRelativeLocation(FVector(25.0, 0.0, 0.0));
				GEditor->SelectActor(Child, true, false, true);
			}
		}
		TStrongObjectPtr<ULevelSequence> Sequence(NewObject<ULevelSequence>(
			GetTransientPackage(), NAME_None, RF_Transactional | RF_Transient));
		Sequence->Initialize();
		FSequencerInitParams Params;
		Params.RootSequence = Sequence.Get();
		Params.PlaybackContext = World;
		Params.ViewParams.UniqueName = TEXT("BlendViewAutoKeyRegression");
		TSharedRef<ISequencer> Sequencer = Module.CreateSequencer(Params);
		const EAutoChangeMode SavedMode = Sequencer->GetAutoChangeMode();
		const EAllowEditsMode SavedEdits = Sequencer->GetAllowEditsMode();
		const bool bSavedDefaults = Sequencer->GetAutoSetTrackDefaults();
		Sequencer->GetSequencerSettings()->SetAutoSetTrackDefaults(false);
		Sequencer->SetAllowEditsMode(EAllowEditsMode::AllEdits);
		Sequencer->SetAutoChangeMode(Case == 6 ? EAutoChangeMode::None : EAutoChangeMode::All);
		const FGuid Binding = Sequencer->GetHandleToObject(Actor);
		const FGuid ChildBinding = Child ? Sequencer->GetHandleToObject(Child) : FGuid();
		FBlendViewViewportContext Context;
		Context.Kind = EBlendViewViewportKind::LevelEditor;
		Context.ViewportClient = GEditor->GetLevelViewportClients()[0];
		Context.Viewport = Context.ViewportClient->Viewport;
		FBlendViewTransformTargetAdapter Adapter;
		FVector Pivot;
		FQuat Rotation;
		TestTrue(TEXT("Capture actor for Sequencer edit"), Adapter.Capture(Context, Pivot, Rotation));
		if (Child)
		{
			for (const FBlendViewActorTransformSnapshot& Snapshot : Adapter.GetActorSnapshots())
			{
				if (Snapshot.Actor.Get() == Child)
				{
					TestEqual(TEXT("Captured child initial relative location"), Snapshot.InitialRelativeTransform.GetLocation().X, 25.0);
				}
			}
		}
		FTransform Final = Actor->GetActorTransform();
		if (Case == 0 || Case == 3 || Case == 6 || Case == 7)
		{
			Final.SetLocation(FVector(120.0, 0.0, 0.0));
		}
		if (Case == 1)
		{
			Final.SetRotation(FRotator(0.0, 45.0, 0.0).Quaternion());
		}
		if (Case == 2)
		{
			Final.SetScale3D(FVector(2.0));
		}
		Actor->SetActorTransform(Final);
		if (Child)
		{
			Child->SetActorRelativeLocation(FVector(35.0, 0.0, 0.0));
		}
		if (Case == 5)
		{
			Adapter.ApplyActorPivotTranslationDelta(FVector(50.0), Pivot, Pivot);
		}
		auto FindTrack = [&]()
		{
			return Sequence->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(Binding, TEXT("Transform"));
		};
		TestNull(TEXT("Preview never creates animation tracks"), FindTrack());
		const FDelegateHandle PreEditObserver = FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddLambda(
			[&](UObject* Object, const FEditPropertyChain& Chain)
			{
				if (Child && Object == Child)
				{
					TestEqual(TEXT("Child pre-edit observers see original local location"), Child->GetRootComponent()->GetRelativeLocation().X, 25.0);
				}
			});
		if (Case == 3)
		{
			Adapter.RestoreInitialTransforms(Pivot, Pivot);
			Adapter.CancelTransaction();
		}
		else
		{
			Adapter.Confirm();
			Adapter.Confirm();
		}
		Adapter.End();
		FCoreUObjectDelegates::OnPreObjectPropertyChanged.Remove(PreEditObserver);
		UMovieScene3DTransformTrack* Track = FindTrack();
		if (Case < 3)
		{
			TestNotNull(TEXT("Confirmed G/R/S creates an auto-keyed transform track"), Track);
			if (Track && !Track->GetAllSections().IsEmpty())
			{
				const int32 ChannelIndex = Case == 0 ? 0 : Case == 1 ? 5 : 6;
				const double Expected = Case == 0 ? 120.0 : Case == 1 ? 45.0 : 2.0;
				UMovieSceneSection* Section = Track->GetAllSections()[0];
				FMovieSceneDoubleChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(ChannelIndex);
				TestNotNull(TEXT("Transform channel exists"), Channel);
				if (Channel)
				{
					TestEqual(TEXT("Repeated confirm does not duplicate keys"), Channel->GetNumKeys(), 1);
					double Value = 0.0;
					Channel->Evaluate(Sequencer->GetLocalTime().Time, Value);
					TestTrue(TEXT("Key records the final transform value"), FMath::IsNearlyEqual(Value, Expected));
					TestTrue(TEXT("Track default preserves the pre-operation value"),
						Channel->GetDefault().IsSet() && FMath::IsNearlyEqual(Channel->GetDefault().GetValue(), Case == 2 ? 1.0 : 0.0));
				}
			}
			TestTrue(TEXT("Confirmation preserves the final actor transform"), Actor->GetActorTransform().Equals(Final));
			if (Case == 0 && Track)
			{
				Sequencer->SetAutoChangeMode(EAutoChangeMode::AutoKey);
				const FFrameTime SecondTime(24000);
				Sequencer->SetLocalTimeDirectly(SecondTime, false);
				FBlendViewTransformTargetAdapter SecondEdit;
				TestTrue(TEXT("Capture a second edit on an existing track"), SecondEdit.Capture(Context, Pivot, Rotation));
				Actor->SetActorLocation(FVector(240.0, 0.0, 0.0));
				SecondEdit.Confirm();
				SecondEdit.End();
				auto KeyCount = [&]()
				{
					UMovieScene3DTransformTrack* Current = FindTrack();
					return Current && !Current->GetAllSections().IsEmpty()
						? Current->GetAllSections()[0]->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(0)->GetNumKeys()
						: 0;
				};
				TestEqual(TEXT("AutoKey adds a second key on an existing track"), KeyCount(), 2);
				GEditor->UndoTransaction();
				TestEqual(TEXT("Undo removes only the second edit's key"), KeyCount(), 1);
				GEditor->RedoTransaction();
				TestEqual(TEXT("Redo restores the second key"), KeyCount(), 2);
			}
		}
		else if (Case == 7 && Child)
		{
			TestNotNull(TEXT("Multi-selection keys the parent"), Track);
			UMovieScene3DTransformTrack* ChildTrack = Sequence->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(ChildBinding, TEXT("Transform"));
			TestNotNull(TEXT("Multi-selection keys the attached child"), ChildTrack);
			TestTrue(TEXT("Parent final position survives batch notifications"), Actor->GetActorLocation().Equals(FVector(120.0, 0.0, 0.0)));
			TestTrue(TEXT("Attached child retains relative, not world, coordinates"), Child->GetRootComponent()->GetRelativeLocation().Equals(FVector(35.0, 0.0, 0.0)));
			if (ChildTrack && !ChildTrack->GetAllSections().IsEmpty())
			{
				FMovieSceneDoubleChannel* Channel = ChildTrack->GetAllSections()[0]->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(0);
				double Value = 0.0;
				Channel->Evaluate(Sequencer->GetLocalTime().Time, Value);
				TestTrue(TEXT("Child animation records relative location"), FMath::IsNearlyEqual(Value, 35.0));
				// Defaults are controlled by Sequencer preferences. Compare with the
				// engine's own property-edit notification path, not an assumed default.
				AActor* Reference = World->SpawnActor<AStaticMeshActor>(Spawn);
				if (Reference)
				{
					Reference->AttachToActor(Actor, FAttachmentTransformRules::KeepRelativeTransform);
					Reference->SetActorRelativeLocation(FVector(25.0, 0.0, 0.0));
					const FGuid ReferenceBinding = Sequencer->GetHandleToObject(Reference);
					FScopedTransaction Transaction(NSLOCTEXT("BlendView", "NativeReference", "Native transform reference"));
					Reference->Modify();
					FProperty* Property = FindFProperty<FProperty>(USceneComponent::StaticClass(), USceneComponent::GetRelativeLocationPropertyName());
					FEditPropertyChain Chain;
					Chain.AddHead(Property);
					FCoreUObjectDelegates::OnPreObjectPropertyChanged.Broadcast(Reference, Chain);
					Reference->SetActorRelativeLocation(FVector(35.0, 0.0, 0.0));
					FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
					FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Reference, Event);
					UMovieScene3DTransformTrack* ReferenceTrack = Sequence->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(ReferenceBinding, TEXT("Transform"));
					TestNotNull(TEXT("Native reference creates a track"), ReferenceTrack);
					if (ReferenceTrack && !ReferenceTrack->GetAllSections().IsEmpty())
					{
						FMovieSceneDoubleChannel* ReferenceChannel = ReferenceTrack->GetAllSections()[0]->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(0);
						TestEqual(TEXT("Child track default matches native property editing"), Channel->GetDefault().Get(0.0), ReferenceChannel->GetDefault().Get(0.0));
					}
					World->DestroyActor(Reference);
				}
			}
		}
		else
		{
			TestNull(TEXT("Cancel, no-op, pivot-only and auto-key off never create a track"), Track);
		}
		Sequencer->SetAutoChangeMode(SavedMode);
		Sequencer->SetAllowEditsMode(SavedEdits);
		Sequencer->GetSequencerSettings()->SetAutoSetTrackDefaults(bSavedDefaults);
		Sequencer->Close();
		GEditor->SelectNone(false, true, false);
		if (Child)
		{
			World->DestroyActor(Child);
		}
		World->DestroyActor(Actor);
	}
	for (AActor* Actor : Selection)
	{
		if (IsValid(Actor))
		{
			GEditor->SelectActor(Actor, true, false, true);
		}
	}
	return true;
}

#endif
