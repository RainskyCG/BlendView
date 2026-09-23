// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "BlendViewSettings.h"
#include "CoreMinimal.h"
#include "EdMode.h"

class FUICommandInfo;

namespace BlendViewQuickFavoriteCatalog
{
	struct FCommandItem
	{
		EBlendViewQuickFavoriteSource SourceType = EBlendViewQuickFavoriteSource::NativeCommand;
		FName StableId = NAME_None;
		FName BindingContext = NAME_None;
		FName CommandName = NAME_None;
		FText Label;
		FText Description;
		FText ContextLabel;
		FText InputText;
	};

	struct FEditorModeFavorite
	{
		FName StableId;
		FEditorModeID ModeId;
		FName RequiredModuleName;
		FText Label;
		FText Description;
		FText InputText;
	};

	struct FActionFavorite
	{
		FName StableId;
		FText Label;
		FText Description;
		FText ContextLabel;
	};

	struct FModeToolFavorite
	{
		FName StableId;
		FName ParentModeStableId;
		FEditorModeID ParentModeId;
		FName RequiredModuleName;
		FName BindingContext;
		FName CommandName;
		FText Label;
		FText Description;
		FText InputText;
	};

	FText ContextDisplayName(EBlendViewQuickFavoriteContext Context);
	bool AllowsContext(const FBlendViewQuickFavoriteCommand& Favorite, EBlendViewQuickFavoriteContext Context);
	FName GetFavoriteStableId(const FBlendViewQuickFavoriteCommand& Favorite);

	bool IsModuleAvailable(FName ModuleName);
	bool IsEditorModeVisibleInFavorites(const FEditorModeFavorite& Favorite);
	bool EnsureEditorModeRegistered(const FEditorModeFavorite& Favorite);

	TOptional<FEditorModeFavorite> FindEditorModeFavorite(FName StableId);
	TOptional<FModeToolFavorite> FindModeToolFavorite(FName StableId);
	TOptional<FActionFavorite> FindBlendViewActionFavorite(FName StableId);

	bool ExecuteBlendViewAction(FName StableId);
	bool MatchesSearch(const FCommandItem& Item, const FString& SearchText);
	TSharedPtr<FUICommandInfo> FindCommand(FName BindingContext, FName CommandName);
	TSharedPtr<FUICommandInfo> FindShortcutCommand(const FCommandItem& Item);

	FCommandItem MakeNativeCommandItem(const TSharedRef<FUICommandInfo>& CommandInfo);
	FCommandItem MakeEditorModeItem(const FEditorModeFavorite& Favorite);
	FCommandItem MakeModeToolItem(const FModeToolFavorite& Favorite);
	FCommandItem MakeBlendViewActionItem(const FActionFavorite& Favorite);

	void GatherAllCommandItems(TArray<FCommandItem>& OutItems, EBlendViewQuickFavoriteContext ActiveContext);
	bool IsDuplicateFavorite(
		const UBlendViewSettings& Settings,
		EBlendViewQuickFavoriteSource SourceType,
		FName StableId,
		EBlendViewQuickFavoriteContext Context);
}
