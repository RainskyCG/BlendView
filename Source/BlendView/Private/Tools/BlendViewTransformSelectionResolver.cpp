// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewTransformSelectionResolver.h"

#include "BlueprintEditor.h"
#include "BlueprintEditorModule.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "SSubobjectEditor.h"
#include "SubobjectDataSubsystem.h"
#include "Viewport/BlendViewViewportContext.h"

namespace
{
	void AddUniqueValidComponent(
		TArray<USceneComponent*>& Components,
		USceneComponent* Component,
		UWorld* ViewportWorld)
	{
		if (IsValid(Component))
		{
			if (AActor* Owner = Component->GetOwner();
				IsValid(Owner) && Owner->GetRootComponent() == Component)
			{
				if (UChildActorComponent* ParentComponent = Owner->GetParentComponent())
				{
					Component = ParentComponent;
				}
			}
		}

		if (IsValid(Component) &&
			Component->GetWorld() == ViewportWorld &&
			!Components.Contains(Component))
		{
			Components.Add(Component);
		}
	}

	struct FResolvedBlueprintSubobjectSelection
	{
		TSharedPtr<FBlueprintEditor> BlueprintEditor;
		TSharedPtr<SSubobjectEditor> SubobjectEditor;
		TWeakObjectPtr<UBlueprint> Blueprint;
		AActor* PreviewActor = nullptr;
		TArray<FSubobjectDataHandle> SelectedHandles;
	};

	bool ResolveBlueprintSubobjectSelectionContext(
		UWorld* ViewportWorld,
		FResolvedBlueprintSubobjectSelection& OutSelection)
	{
		if (!ViewportWorld || !FModuleManager::Get().IsModuleLoaded(TEXT("Kismet")))
		{
			return false;
		}

		FBlueprintEditorModule& BlueprintEditorModule =
			FModuleManager::GetModuleChecked<FBlueprintEditorModule>(TEXT("Kismet"));
		for (const TSharedRef<IBlueprintEditor>& BlueprintEditorInterface :
			BlueprintEditorModule.GetBlueprintEditors())
		{
			const TSharedRef<FBlueprintEditor> BlueprintEditor =
				StaticCastSharedRef<FBlueprintEditor>(BlueprintEditorInterface);
			AActor* PreviewActor = BlueprintEditor->GetPreviewActor();
			if (!IsValid(PreviewActor) || PreviewActor->GetWorld() != ViewportWorld)
			{
				continue;
			}

			const TSharedPtr<SSubobjectEditor> SubobjectEditor =
				BlueprintEditor->GetSubobjectEditor();
			if (!SubobjectEditor.IsValid())
			{
				continue;
			}

			OutSelection.BlueprintEditor = BlueprintEditor;
			OutSelection.SubobjectEditor = SubobjectEditor;
			OutSelection.Blueprint = BlueprintEditor->GetBlueprintObj();
			OutSelection.PreviewActor = PreviewActor;
			OutSelection.SelectedHandles = SubobjectEditor->GetSelectedHandles();
			return true;
		}

		return false;
	}

	bool ResolveBlueprintSubobjectSelection(
		UWorld* ViewportWorld,
		TArray<USceneComponent*>& OutComponents)
	{
		FResolvedBlueprintSubobjectSelection Selection;
		if (!ResolveBlueprintSubobjectSelectionContext(ViewportWorld, Selection))
		{
			return false;
		}

		for (const FSubobjectDataHandle& Handle : Selection.SelectedHandles)
		{
			const FSubobjectData* Data = Handle.GetData();
			USceneComponent* Component = Data
				? Cast<USceneComponent>(
					const_cast<UActorComponent*>(
						Data->FindComponentInstanceInActor(Selection.PreviewActor)))
				: nullptr;
			AddUniqueValidComponent(OutComponents, Component, ViewportWorld);
		}
		return true;
	}
}

void FBlendViewTransformSelectionResolver::ResolveNativeSceneComponents(
	const FBlendViewViewportContext& Context,
	TArray<USceneComponent*>& OutComponents)
{
	OutComponents.Reset();
	if (!GEditor || !Context.ViewportClient)
	{
		return;
	}

	UWorld* ViewportWorld = Context.ViewportClient->GetWorld();
	if (!ViewportWorld)
	{
		return;
	}

	// Blueprint preview selection is owned by the Subobject Editor and is not
	// guaranteed to be mirrored into GEditor's global component selection.
	if (ResolveBlueprintSubobjectSelection(ViewportWorld, OutComponents))
	{
		return;
	}

	if (USelection* SelectedComponents = GEditor->GetSelectedComponents())
	{
		TArray<USceneComponent*> SceneComponents;
		SelectedComponents->GetSelectedObjects<USceneComponent>(SceneComponents);
		for (USceneComponent* Component : SceneComponents)
		{
			AddUniqueValidComponent(OutComponents, Component, ViewportWorld);
		}
	}

	for (TActorIterator<AActor> ActorIt(ViewportWorld); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		ActorIt->GetComponents(PrimitiveComponents);
		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (IsValid(Component) && Component->IsComponentIndividuallySelected())
			{
				AddUniqueValidComponent(OutComponents, Component, ViewportWorld);
			}
		}
	}
}

bool FBlendViewTransformSelectionResolver::DuplicateBlueprintSubobjects(
	const FBlendViewViewportContext& Context,
	TArray<TWeakObjectPtr<USceneComponent>>& OutDuplicatedComponents,
	TUniquePtr<FScopedTransaction>& OutTransaction)
{
	OutDuplicatedComponents.Reset();
	OutTransaction.Reset();
	if (!Context.ViewportClient)
	{
		return false;
	}

	FResolvedBlueprintSubobjectSelection Selection;
	if (!ResolveBlueprintSubobjectSelectionContext(
		Context.ViewportClient->GetWorld(),
		Selection) ||
		Selection.SelectedHandles.IsEmpty() ||
		!Selection.Blueprint.IsValid())
	{
		return false;
	}

	USubobjectDataSubsystem* SubobjectSystem = USubobjectDataSubsystem::Get();
	if (!SubobjectSystem ||
		!SubobjectSystem->CanCopySubobjects(Selection.SelectedHandles))
	{
		return false;
	}

	OutTransaction = MakeUnique<FScopedTransaction>(
		Selection.SelectedHandles.Num() > 1
			? NSLOCTEXT("BlendView", "DuplicateComponents", "BlendView Duplicate Components")
			: NSLOCTEXT("BlendView", "DuplicateComponent", "BlendView Duplicate Component"));

	const FSubobjectDataHandle ContextHandle =
		Selection.SubobjectEditor->GetObjectContextHandle();
	TArray<FSubobjectDataHandle> DuplicatedHandles;
	SubobjectSystem->DuplicateSubobjects(
		ContextHandle,
		Selection.SelectedHandles,
		Selection.Blueprint.Get(),
		DuplicatedHandles);
	if (DuplicatedHandles.IsEmpty())
	{
		OutTransaction->Cancel();
		OutTransaction.Reset();
		return false;
	}

	Selection.SubobjectEditor->UpdateTree();
	Selection.SubobjectEditor->ClearSelection();
	for (const FSubobjectDataHandle& Handle : DuplicatedHandles)
	{
		Selection.SubobjectEditor->SelectNodeFromHandle(Handle, true);
		const FSubobjectData* Data = Handle.GetData();
		USceneComponent* Component = Data
			? Cast<USceneComponent>(
				const_cast<UActorComponent*>(
					Data->FindComponentInstanceInActor(Selection.PreviewActor)))
			: nullptr;
		TArray<USceneComponent*> NormalizedComponents;
		AddUniqueValidComponent(
			NormalizedComponents,
			Component,
			Context.ViewportClient->GetWorld());
		for (USceneComponent* NormalizedComponent : NormalizedComponents)
		{
			OutDuplicatedComponents.AddUnique(NormalizedComponent);
		}
	}

	return true;
}
