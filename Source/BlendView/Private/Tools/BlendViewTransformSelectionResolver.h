// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FBlendViewViewportContext;
class FScopedTransaction;
class USceneComponent;

class FBlendViewTransformSelectionResolver
{
public:
	static void ResolveNativeSceneComponents(
		const FBlendViewViewportContext& Context,
		TArray<USceneComponent*>& OutComponents);
	static bool DuplicateBlueprintSubobjects(
		const FBlendViewViewportContext& Context,
		TArray<TWeakObjectPtr<USceneComponent>>& OutDuplicatedComponents,
		TUniquePtr<FScopedTransaction>& OutTransaction);
};
