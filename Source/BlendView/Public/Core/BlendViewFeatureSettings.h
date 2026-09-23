// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBoolProperty;
class UBlendViewSettings;

struct FBlendViewFeatureToggleDefinition
{
	FName Id;
	FName PropertyName;
	const TCHAR* ChineseName = TEXT("");
	const TCHAR* EnglishName = TEXT("");
	const TCHAR* ChineseTooltip = TEXT("");
	const TCHAR* EnglishTooltip = TEXT("");
	FName IconStyleSetName = NAME_None;
	FName IconName = NAME_None;

	FText GetDisplayName() const;
	FText GetTooltip() const;
};

class FBlendViewFeatureSettings final
{
public:
	static TConstArrayView<FBlendViewFeatureToggleDefinition> GetExperimentalFeatures();
	static const FBlendViewFeatureToggleDefinition* FindExperimentalFeature(FName PropertyName);
	static bool IsEnabled(const UBlendViewSettings* Settings, FName PropertyName);
	static bool Toggle(UBlendViewSettings* Settings, FName PropertyName);

private:
	static FBoolProperty* FindBoolProperty(FName PropertyName);
};