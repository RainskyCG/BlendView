// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Localization/BlendViewLocalization.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlendViewLocalizationResolutionTest,
	"BlendView.Localization.LanguageResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlendViewLocalizationResolutionTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Auto recognizes Simplified Chinese"),
		FBlendViewLocalization::ResolveLanguage(EBlendViewDisplayLanguage::Auto, TEXT("zh-Hans-CN")),
		EBlendViewResolvedLanguage::Chinese);
	TestEqual(
		TEXT("Auto recognizes Traditional Chinese"),
		FBlendViewLocalization::ResolveLanguage(EBlendViewDisplayLanguage::Auto, TEXT("zh-Hant-TW")),
		EBlendViewResolvedLanguage::Chinese);
	TestEqual(
		TEXT("Auto falls back to English for other editor languages"),
		FBlendViewLocalization::ResolveLanguage(EBlendViewDisplayLanguage::Auto, TEXT("ja-JP")),
		EBlendViewResolvedLanguage::English);
	TestEqual(
		TEXT("Chinese override ignores editor language"),
		FBlendViewLocalization::ResolveLanguage(EBlendViewDisplayLanguage::Chinese, TEXT("en-US")),
		EBlendViewResolvedLanguage::Chinese);
	TestEqual(
		TEXT("English override ignores editor language"),
		FBlendViewLocalization::ResolveLanguage(EBlendViewDisplayLanguage::English, TEXT("zh-CN")),
		EBlendViewResolvedLanguage::English);
	return true;
}

#endif
