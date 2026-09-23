// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlendViewSettings.h"

enum class EBlendViewResolvedLanguage : uint8
{
	Chinese,
	English
};

class FBlendViewLocalization
{
public:
	static EBlendViewResolvedLanguage GetResolvedLanguage();
	static EBlendViewResolvedLanguage ResolveLanguage(
		EBlendViewDisplayLanguage DisplayLanguage,
		const FString& EditorCultureName);
	static FText Text(const TCHAR* Chinese, const TCHAR* English);
};
