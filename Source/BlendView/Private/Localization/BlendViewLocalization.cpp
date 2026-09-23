// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Localization/BlendViewLocalization.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

EBlendViewResolvedLanguage FBlendViewLocalization::GetResolvedLanguage()
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	const EBlendViewDisplayLanguage DisplayLanguage = Settings
		? Settings->DisplayLanguage
		: EBlendViewDisplayLanguage::Auto;
	const FString CultureName = FInternationalization::Get().GetCurrentLanguage()->GetName();
	return ResolveLanguage(DisplayLanguage, CultureName);
}

EBlendViewResolvedLanguage FBlendViewLocalization::ResolveLanguage(
	const EBlendViewDisplayLanguage DisplayLanguage,
	const FString& EditorCultureName)
{
	if (DisplayLanguage == EBlendViewDisplayLanguage::Chinese)
	{
		return EBlendViewResolvedLanguage::Chinese;
	}
	if (DisplayLanguage == EBlendViewDisplayLanguage::English)
	{
		return EBlendViewResolvedLanguage::English;
	}

	return EditorCultureName.StartsWith(TEXT("zh"), ESearchCase::IgnoreCase)
		? EBlendViewResolvedLanguage::Chinese
		: EBlendViewResolvedLanguage::English;
}

FText FBlendViewLocalization::Text(const TCHAR* Chinese, const TCHAR* English)
{
	return FText::FromString(
		GetResolvedLanguage() == EBlendViewResolvedLanguage::Chinese ? Chinese : English);
}
