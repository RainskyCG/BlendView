// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FBlendViewInputProcessor;
class FBlendViewActiveSelectionTracker;
class FMenuBuilder;
class FSlateStyleSet;
class APlayerController;
class UCanvas;
class SWidget;
class UToolMenu;
enum class EBlendViewSnapSourceMode : uint8;
enum class EBlendViewSnapTargetKind : uint8;
enum class EBlendViewTransformPivotMode : uint8;

class FBlendViewModule final : public IModuleInterface
{
public:
	virtual ~FBlendViewModule() override;
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterStyle();
	void UnregisterStyle();
	void RegisterMenus();
	void AddBlendViewToolbarEntries(UToolMenu& ToolbarMenu, FName InsertAfterSectionName);
	void AddBlendViewViewportToolbarEntries(UToolMenu& ToolbarMenu);
	TSharedRef<SWidget> GenerateOptionsMenu();
	TSharedRef<SWidget> GenerateTransformPivotToolbarMenu();
	TSharedRef<SWidget> GenerateSnapSettingsToolbarMenu();
	void PopulateSnapSettingsMenu(FMenuBuilder& MenuBuilder);
	void PopulateTransformPivotMenu(FMenuBuilder& MenuBuilder);
	void PopulateExperimentalFeaturesMenu(FMenuBuilder& MenuBuilder);
	FText GetTransformPivotToolbarLabel() const;
	FText GetTransformPivotToolbarTooltip() const;
	FSlateIcon GetTransformPivotToolbarIcon() const;
	FText GetSnapSettingsToolbarLabel() const;
	FText GetSnapSettingsToolbarTooltip() const;
	void ToggleBlendViewEnabled();
	bool IsBlendViewEnabled() const;
	void ToggleTransformWorkflow();
	bool IsTransformWorkflowEnabled() const;
	void ToggleMouseNavigation();
	bool IsMouseNavigationEnabled() const;
	void ToggleCenterToolbar();
	bool IsCenterToolbarVisible() const;
	void ToggleRightMouseFlyBoost();
	bool IsRightMouseFlyBoostEnabled() const;
	void HandleExperimentalFeatureToggled(FName PropertyName);
	void TogglePersistentSnap();
	bool IsPersistentSnapEnabled() const;
	void OpenPluginSettings() const;
	void ToggleUnrealEditorSnapCapture();
	bool IsUnrealEditorSnapCaptureEnabled() const;
	void ToggleStructuralEdgeLimit();
	bool IsStructuralEdgeLimitEnabled() const;
	void ToggleAlignRotationToSnapTarget();
	bool IsAlignRotationToSnapTargetEnabled() const;
	void SetSnapSourceMode(EBlendViewSnapSourceMode Mode);
	bool IsSnapSourceMode(EBlendViewSnapSourceMode Mode) const;
	void SetTransformPivotMode(EBlendViewTransformPivotMode Mode);
	bool IsTransformPivotMode(EBlendViewTransformPivotMode Mode) const;
	void ToggleSnapTarget(EBlendViewSnapTargetKind TargetKind);
	bool IsSnapTargetEnabled(EBlendViewSnapTargetKind TargetKind) const;
	void RegisterInputProcessor();
	void UnregisterInputProcessor();
	void PrepareForEngineExit();
	void OnBeginPIE(bool bIsSimulating);
	void OnEndPIE(bool bIsSimulating);
	void OnDebugDraw(UCanvas* Canvas, APlayerController* PlayerController);

	TSharedPtr<FBlendViewInputProcessor> InputProcessor;
	TUniquePtr<FBlendViewActiveSelectionTracker> ActiveSelectionTracker;
	TSharedPtr<FSlateStyleSet> StyleSet;
	FDelegateHandle ToolMenusStartupHandle;
	FDelegateHandle BeginPIEDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;
	FDelegateHandle DebugDrawDelegateHandle;
	FDelegateHandle EnginePreExitDelegateHandle;
	bool bInputProcessorRegistered = false;
	bool bWasRegisteredBeforePIE = false;
	bool bPreparedForEngineExit = false;
};
