// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FKeyEvent;
class FSlateApplication;
class FUICommandList;

using FBlendViewQuickFavoriteCommandListMap = TMap<FName, TArray<TWeakPtr<FUICommandList>>>;

class FBlendViewQuickFavorites final
{
public:
	FBlendViewQuickFavorites();
	~FBlendViewQuickFavorites();

	bool TryOpen(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent);
	bool TryOpenSearch(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent);

private:
	bool TryOpenMenu(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent, bool bCommandSearch);
	void HandleRegisterCommandList(FName BindingContext, TSharedRef<FUICommandList> CommandList);
	void HandleUnregisterCommandList(FName BindingContext, TSharedRef<FUICommandList> CommandList);

	FBlendViewQuickFavoriteCommandListMap CommandListsByContext;
	FDelegateHandle RegisterCommandListHandle;
	FDelegateHandle UnregisterCommandListHandle;
};
