// Copyright 2026 RainskyCG. All Rights Reserved.

#include "BlendViewSettings.h"

#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "BlendViewSettings"

UBlendViewSettings::UBlendViewSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("BlendView");
}

void UBlendViewSettings::PostInitProperties()
{
	Super::PostInitProperties();

	constexpr int32 CurrentSchemaVersion = 1;
	if (!HasAnyFlags(RF_ClassDefaultObject) || SettingsSchemaVersion >= CurrentSchemaVersion || !GConfig)
	{
		return;
	}

	const TCHAR* Section = TEXT("/Script/BlendView.BlendViewSettings");
	auto MigrateBool = [this, Section](const TCHAR* Key, bool& Value)
	{
		GConfig->GetBool(Section, Key, Value, GEditorPerProjectIni);
	};
	auto MigrateFloat = [this, Section](const TCHAR* Key, float& Value)
	{
		GConfig->GetFloat(Section, Key, Value, GEditorPerProjectIni);
	};

	MigrateBool(TEXT("bEnableTransformWorkflow"), bEnableTransformWorkflow);
	MigrateBool(TEXT("bEnableMouseNavigation"), bEnableMouseNavigation);
	MigrateFloat(TEXT("OrbitSensitivity"), OrbitSensitivity);
	MigrateBool(TEXT("bInvertOrbitYAxis"), bInvertOrbitYAxis);
	MigrateBool(TEXT("bEnableShiftFlySpeedBoost"), bEnableShiftFlySpeedBoost);
	MigrateBool(TEXT("bEnableUnrealEditorSnapCapture"), bEnableUnrealEditorSnapCapture);
	MigrateBool(TEXT("bEnableStructuralEdgeLimit"), bEnableStructuralEdgeLimit);
	MigrateBool(TEXT("bSnapTargetGrid"), bSnapTargetGrid);
	MigrateBool(TEXT("bSnapTargetVertex"), bSnapTargetVertex);
	MigrateBool(TEXT("bSnapTargetEdge"), bSnapTargetEdge);
	MigrateBool(TEXT("bSnapTargetEdgeMidpoint"), bSnapTargetEdgeMidpoint);
	MigrateBool(TEXT("bSnapTargetFace"), bSnapTargetFace);
	FString EnumValue;
	if (GConfig->GetString(Section, TEXT("SnapSourceMode"), EnumValue, GEditorPerProjectIni))
	{
		SnapSourceMode = EnumValue.EndsWith(TEXT("Closest"))
			? EBlendViewSnapSourceMode::Closest
			: EBlendViewSnapSourceMode::Pivot;
	}
	if (GConfig->GetString(Section, TEXT("TranslationNumericUnit"), EnumValue, GEditorPerProjectIni))
	{
		TranslationNumericUnit = EnumValue.EndsWith(TEXT("Centimeters"))
			? EBlendViewTranslationNumericUnit::Centimeters
			: EBlendViewTranslationNumericUnit::Meters;
	}

	SettingsSchemaVersion = CurrentSchemaVersion;
	SaveConfig();
}

FName UBlendViewSettings::GetContainerName() const
{
	return TEXT("Editor");
}

FName UBlendViewSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText UBlendViewSettings::GetSectionText() const
{
	return LOCTEXT("SettingsSection", "BlendView");
}

double UBlendViewSettings::GetTranslationNumericUnitScale() const
{
	return TranslationNumericUnit == EBlendViewTranslationNumericUnit::Meters ? 100.0 : 1.0;
}

#undef LOCTEXT_NAMESPACE
