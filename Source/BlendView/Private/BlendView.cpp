// Copyright 2026 RainskyCG. All Rights Reserved.

#include "BlendView.h"

#include "BlendViewCommands.h"
#include "BlendViewSettings.h"
#include "BlendViewSettingsCustomization.h"
#include "Core/BlendViewSessionState.h"
#include "Core/BlendViewFeatureSettings.h"
#include "Debug/DebugDrawService.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Graph/BlendViewGraphCommands.h"
#include "ISettingsModule.h"
#include "Interfaces/IPluginManager.h"
#include "Input/BlendViewInputProcessor.h"
#include "Localization/BlendViewLocalization.h"
#include "Misc/CoreDelegates.h"
#include "Brushes/SlateImageBrush.h"
#include "Selection/BlendViewActiveSelectionTracker.h"
#include "Snap/BlendViewSnapSolver.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/ToolBarStyle.h"
#include "ToolMenus.h"
#include "UI/SBlendViewViewportToolbarControls.h"
#include "UI/BlendViewStatusBarPresenter.h"

#define LOCTEXT_NAMESPACE "FBlendViewModule"

namespace
{
	const FName BlendViewStyleSetName(TEXT("BlendViewStyle"));
	const FName BlendViewToolbarIconName(TEXT("BlendView.ToggleIcon"));
	const FName BlendViewPivotBoundingBoxCenterIconName(TEXT("BlendView.PivotBoundingBoxCenter"));
	const FName BlendViewPivotCursorIconName(TEXT("BlendView.PivotCursor"));
	const FName BlendViewPivotIndividualOriginsIconName(TEXT("BlendView.PivotIndividualOrigins"));
	const FName BlendViewPivotActiveItemIconName(TEXT("BlendView.PivotActiveItem"));
	const FName BlendViewViewportComboButtonStyleName(TEXT("BlendView.ViewportComboButton"));
	const FName BlendViewViewportButtonStyleName(TEXT("BlendView.ViewportComboButton.Button"));
}

FBlendViewModule::~FBlendViewModule() = default;

void FBlendViewModule::StartupModule()
{
	RegisterStyle();
	FBlendViewCommands::Register();
	FBlendViewGraphCommands::Register();
	RegisterBlendViewSettingsCustomization();
	InputProcessor = MakeShared<FBlendViewInputProcessor>();
	RegisterInputProcessor();
	ActiveSelectionTracker = MakeUnique<FBlendViewActiveSelectionTracker>();
	ActiveSelectionTracker->Startup();
	EnginePreExitDelegateHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FBlendViewModule::PrepareForEngineExit);
	DebugDrawDelegateHandle = UDebugDrawService::Register(
		TEXT("ModeWidgets"),
		FDebugDrawDelegate::CreateRaw(this, &FBlendViewModule::OnDebugDraw));

	BeginPIEDelegateHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FBlendViewModule::OnBeginPIE);
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddRaw(this, &FBlendViewModule::OnEndPIE);
	ToolMenusStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlendViewModule::RegisterMenus));
}

void FBlendViewModule::ShutdownModule()
{
	if (EnginePreExitDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitDelegateHandle);
		EnginePreExitDelegateHandle.Reset();
	}

	const bool bEngineExit = IsEngineExitRequested();
	if (!bEngineExit)
	{
		FBlendViewStatusBarPresenter::Get().Restore();
	}

	if (ToolMenusStartupHandle.IsValid())
	{
		UToolMenus::UnRegisterStartupCallback(ToolMenusStartupHandle);
		ToolMenusStartupHandle.Reset();
	}
	UToolMenus::UnregisterOwner(this);

	if (BeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(BeginPIEDelegateHandle);
		BeginPIEDelegateHandle.Reset();
	}
	if (EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
		EndPIEDelegateHandle.Reset();
	}

	if (DebugDrawDelegateHandle.IsValid())
	{
		UDebugDrawService::Unregister(DebugDrawDelegateHandle);
		DebugDrawDelegateHandle.Reset();
	}

	if (InputProcessor.IsValid() && !bEngineExit)
	{
		InputProcessor->CancelActiveOperation();
	}

	UnregisterInputProcessor();
	InputProcessor.Reset();
	if (ActiveSelectionTracker.IsValid())
	{
		ActiveSelectionTracker->Shutdown();
		ActiveSelectionTracker.Reset();
	}

	UnregisterBlendViewSettingsCustomization();
	FBlendViewGraphCommands::Unregister();
	FBlendViewCommands::Unregister();
	UnregisterStyle();
}

void FBlendViewModule::RegisterStyle()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(BlendViewStyleSetName);

	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlendView")))
	{
		StyleSet->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));
	}

	StyleSet->Set(
		BlendViewToolbarIconName,
		new FSlateVectorImageBrush(
			StyleSet->RootToContentDir(TEXT("BlendViewIcon"), TEXT(".svg")),
			FVector2D(28.0, 28.0)));
	StyleSet->Set(
		BlendViewPivotBoundingBoxCenterIconName,
		new FSlateVectorImageBrush(
			StyleSet->RootToContentDir(TEXT("PivotBoundingBoxCenter"), TEXT(".svg")),
			FVector2D(16.0, 16.0)));
	StyleSet->Set(
		BlendViewPivotCursorIconName,
		new FSlateVectorImageBrush(
			StyleSet->RootToContentDir(TEXT("PivotCursor"), TEXT(".svg")),
			FVector2D(16.0, 16.0)));
	StyleSet->Set(
		BlendViewPivotIndividualOriginsIconName,
		new FSlateVectorImageBrush(
			StyleSet->RootToContentDir(TEXT("PivotIndividualOrigins"), TEXT(".svg")),
			FVector2D(16.0, 16.0)));
	StyleSet->Set(
		BlendViewPivotActiveItemIconName,
		new FSlateVectorImageBrush(
			StyleSet->RootToContentDir(TEXT("PivotActiveItem"), TEXT(".svg")),
			FVector2D(16.0, 16.0)));
	const FToolBarStyle& ViewportToolbarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>(TEXT("ViewportToolbar"));
	const FSlateColor BlendViewButtonBackground =
		FSlateColor(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("0F0F0F"))));

	FSlateBrush NormalBrush = ViewportToolbarStyle.ComboButtonStyle.ButtonStyle.Normal;
	FSlateBrush HoveredBrush = ViewportToolbarStyle.ComboButtonStyle.ButtonStyle.Hovered;
	FSlateBrush PressedBrush = ViewportToolbarStyle.ComboButtonStyle.ButtonStyle.Pressed;
	NormalBrush.TintColor = BlendViewButtonBackground;
	HoveredBrush.TintColor = BlendViewButtonBackground;
	PressedBrush.TintColor = BlendViewButtonBackground;

	const FButtonStyle BlendViewViewportButtonStyle = FButtonStyle(ViewportToolbarStyle.ComboButtonStyle.ButtonStyle)
		.SetNormal(NormalBrush)
		.SetHovered(HoveredBrush)
		.SetPressed(PressedBrush);
	StyleSet->Set(BlendViewViewportButtonStyleName, BlendViewViewportButtonStyle);

	const FComboButtonStyle BlendViewViewportComboButtonStyle =
		FComboButtonStyle(ViewportToolbarStyle.ComboButtonStyle).SetButtonStyle(BlendViewViewportButtonStyle);
	StyleSet->Set(BlendViewViewportComboButtonStyleName, BlendViewViewportComboButtonStyle);
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FBlendViewModule::UnregisterStyle()
{
	if (!StyleSet.IsValid())
	{
		return;
	}

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
}

void FBlendViewModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.AssetsToolBar"));
	if (ToolbarMenu)
	{
		AddBlendViewToolbarEntries(*ToolbarMenu, TEXT("Content"));
	}

	if (UToolMenu* AssetToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.DefaultToolBar")))
	{
		AddBlendViewToolbarEntries(*AssetToolbarMenu, TEXT("Asset"));
	}

	if (UToolMenu* ViewportToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.ViewportToolbar")))
	{
		AddBlendViewViewportToolbarEntries(*ViewportToolbarMenu);
	}

	if (UToolMenu* StatusBarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.StatusBar.ToolBar")))
	{
		FToolMenuSection& StatusSection = StatusBarMenu->AddSection(
			TEXT("BlendViewTransformHints"),
			FText::GetEmpty(),
			FToolMenuInsert(TEXT("SourceControl"), EToolMenuInsertType::Before));
		StatusSection.AddEntry(FToolMenuEntry::InitWidget(
			TEXT("BlendViewTransformHints"),
			FBlendViewStatusBarPresenter::Get().CreateFallbackWidget(),
			FText::GetEmpty(),
			true,
			false));
	}
}

void FBlendViewModule::AddBlendViewToolbarEntries(
	UToolMenu& ToolbarMenu,
	const FName InsertAfterSectionName)
{
	FToolMenuSection& Section = ToolbarMenu.AddSection(
		TEXT("BlendView"),
		FText::GetEmpty(),
		FToolMenuInsert(InsertAfterSectionName, EToolMenuInsertType::After));

	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		TEXT("BlendViewToggle"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleBlendViewEnabled),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsBlendViewEnabled)),
		FText::GetEmpty(),
		FBlendViewLocalization::Text(
			TEXT("\u4E34\u65F6\u542F\u7528\u6216\u7981\u7528\u6574\u4E2A BlendView \u63D2\u4EF6\u3002"),
			TEXT("Temporarily enable or disable the entire BlendView plugin.")),
		FSlateIcon(BlendViewStyleSetName, BlendViewToolbarIconName),
		EUserInterfaceActionType::ToggleButton));

	Section.AddEntry(FToolMenuEntry::InitComboButton(
		TEXT("BlendViewOptions"),
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FBlendViewModule::GenerateOptionsMenu),
		FText::GetEmpty(),
		FBlendViewLocalization::Text(
			TEXT("BlendView \u529F\u80FD\u9009\u9879\u3002"),
			TEXT("BlendView options.")),
		FSlateIcon(),
		true));
}

void FBlendViewModule::AddBlendViewViewportToolbarEntries(UToolMenu& ToolbarMenu)
{
	if (ToolbarMenu.ContainsEntry(TEXT("BlendViewViewportToolbarControls")) ||
		ToolbarMenu.ContainsEntry(TEXT("BlendViewTransformPivot")) ||
		ToolbarMenu.ContainsEntry(TEXT("BlendViewSnapSettings")))
	{
		return;
	}

	FToolMenuSection& Section = ToolbarMenu.FindOrAddSection(TEXT("BlendViewViewportCenter"));
	Section.Alignment = EToolMenuSectionAlign::Middle;

	const FComboButtonStyle& BlendViewComboButtonStyle =
		StyleSet->GetWidgetStyle<FComboButtonStyle>(BlendViewViewportComboButtonStyleName);
	const FButtonStyle& BlendViewButtonStyle =
		StyleSet->GetWidgetStyle<FButtonStyle>(BlendViewViewportButtonStyleName);

	TSharedRef<SWidget> ToolbarControls =
		SNew(SBlendViewViewportToolbarControls)
		.Visibility_Lambda([this]()
		{
			return IsCenterToolbarVisible()
				? EVisibility::Visible
				: EVisibility::Collapsed;
		})
		.ComboButtonStyle(&BlendViewComboButtonStyle)
		.ButtonStyle(&BlendViewButtonStyle)
		.PivotIcon_Lambda([this]()
		{
			return GetTransformPivotToolbarIcon().GetIcon();
		})
		.PivotToolTip(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(
			this,
			&FBlendViewModule::GetTransformPivotToolbarTooltip)))
		.SnapIcon(FAppStyle::GetBrush(TEXT("ViewportToolbar.Snap")))
		.SnapToolTip(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(
			this,
			&FBlendViewModule::GetSnapSettingsToolbarTooltip)))
		.SnapTargetLabel(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(
			this,
			&FBlendViewModule::GetSnapSettingsToolbarLabel)))
		.OnGetPivotMenuContent(FOnGetContent::CreateRaw(this, &FBlendViewModule::GenerateTransformPivotToolbarMenu))
		.OnGetSnapMenuContent(FOnGetContent::CreateRaw(this, &FBlendViewModule::GenerateSnapSettingsToolbarMenu))
		.OnTogglePersistentSnap(FSimpleDelegate::CreateRaw(this, &FBlendViewModule::TogglePersistentSnap))
		.IsPersistentSnapEnabled(FBlendViewIsPersistentSnapEnabled::CreateRaw(
			this,
			&FBlendViewModule::IsPersistentSnapEnabled));

	FToolMenuEntry& ControlsEntry = Section.AddEntry(FToolMenuEntry::InitWidget(
		TEXT("BlendViewViewportToolbarControls"),
		ToolbarControls,
		FText::GetEmpty(),
		true,
		false,
		true));
	ControlsEntry.SetShowInToolbarTopLevel(true);
	ControlsEntry.ToolBarData.LabelOverride = FText::GetEmpty();
	ControlsEntry.ToolBarData.ResizeParams.AllowClipping = false;
	ControlsEntry.ToolBarData.ResizeParams.ClippingPriority = -100;
	ControlsEntry.ToolBarData.ResizeParams.VisibleInOverflow = true;
	ControlsEntry.WidgetData.ResizeParams = ControlsEntry.ToolBarData.ResizeParams;
}

TSharedRef<SWidget> FBlendViewModule::GenerateOptionsMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(TEXT("BlendViewFeatures"), FBlendViewLocalization::Text(TEXT("\u529F\u80FD"), TEXT("Features")));
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u53D8\u6362\u5DE5\u4F5C\u6D41"), TEXT("Transform Workflow")),
		FBlendViewLocalization::Text(TEXT("\u542F\u7528 Blender \u98CE\u683C G/R/S \u53D8\u6362\u5DE5\u4F5C\u6D41\u3002"), TEXT("Enable the Blender-style G/R/S transform workflow.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleTransformWorkflow),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsTransformWorkflowEnabled)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u9F20\u6807\u89C6\u56FE\u5BFC\u822A"), TEXT("Mouse View Navigation")),
		FBlendViewLocalization::Text(TEXT("\u542F\u7528 MMB\u3001Shift+MMB \u548C Ctrl+MMB \u89C6\u56FE\u5BFC\u822A\u3002"), TEXT("Enable MMB, Shift+MMB, and Ctrl+MMB view navigation.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleMouseNavigation),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsMouseNavigationEnabled)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u663E\u793A\u5C45\u4E2D\u5DE5\u5177\u680F"), TEXT("Show Center Toolbar")),
		FBlendViewLocalization::Text(
			TEXT("\u5728\u5173\u5361\u89C6\u53E3\u9876\u90E8\u5C45\u4E2D\u663E\u793A BlendView \u7684\u8F74\u5FC3\u70B9\u548C\u5438\u9644\u5FEB\u6377\u5DE5\u5177\u680F\u3002"),
			TEXT("Show BlendView pivot and snapping shortcut controls centered at the top of the level viewport.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleCenterToolbar),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsCenterToolbarVisible)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u53F3\u952E\u6F2B\u6E38\u901F\u5EA6\u8C03\u6574"), TEXT("Right-Mouse Fly Speed")),
		FBlendViewLocalization::Text(TEXT("\u542F\u7528\u53F3\u952E WASDQE \u6F2B\u6E38\u65F6\u7684 Shift \u52A0\u901F\u548C Alt \u51CF\u901F\u3002"), TEXT("Enable Shift acceleration and Alt slow-down during right-mouse WASDQE fly navigation.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleRightMouseFlyBoost),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsRightMouseFlyBoostEnabled)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddSubMenu(
		FBlendViewLocalization::Text(TEXT("\u5B9E\u9A8C\u529F\u80FD"), TEXT("Experimental Features")),
		FBlendViewLocalization::Text(TEXT("\u542F\u7528\u6216\u7981\u7528 BlendView \u7684\u5B9E\u9A8C\u6027\u529F\u80FD\u6A21\u5757\u3002"), TEXT("Enable or disable BlendView experimental feature modules.")),
		FNewMenuDelegate::CreateRaw(this, &FBlendViewModule::PopulateExperimentalFeaturesMenu),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Experimental")));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("BlendViewSettings"), FBlendViewLocalization::Text(TEXT("\u8BBE\u7F6E"), TEXT("Settings")));
	MenuBuilder.AddSubMenu(
		FBlendViewLocalization::Text(TEXT("\u53D8\u6362\u8F74\u5FC3\u70B9"), TEXT("Transform Pivot")),
		FBlendViewLocalization::Text(
			TEXT("\u8BBE\u7F6E\u65CB\u8F6C\u3001\u7F29\u653E\u548C\u955C\u50CF\u65F6\u4F7F\u7528\u7684\u53D8\u6362\u8F74\u5FC3\u70B9\u3002"),
			TEXT("Choose the pivot used by rotation, scaling, and mirroring.")),
		FNewMenuDelegate::CreateRaw(this, &FBlendViewModule::PopulateTransformPivotMenu),
		false,
		FSlateIcon(BlendViewStyleSetName, BlendViewPivotBoundingBoxCenterIconName));
	MenuBuilder.AddSubMenu(
		FBlendViewLocalization::Text(TEXT("\u5438\u9644\u8BBE\u7F6E"), TEXT("Snapping Settings")),
		FBlendViewLocalization::Text(
			TEXT("\u8BBE\u7F6E\u5438\u9644\u884C\u4E3A\u3001\u5438\u9644\u57FA\u51C6\u548C\u53EF\u7528\u5438\u9644\u76EE\u6807\u3002"),
			TEXT("Configure snapping behavior, snap source, and available snap targets.")),
		FNewMenuDelegate::CreateRaw(this, &FBlendViewModule::PopulateSnapSettingsMenu),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ViewportToolbar.Snap")));
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u8BBE\u7F6E"), TEXT("Settings")),
		FBlendViewLocalization::Text(TEXT("\u6253\u5F00\u7F16\u8F91\u5668\u9996\u9009\u9879\u4E2D\u7684 BlendView \u8BBE\u7F6E\u9875\u9762\u3002"), TEXT("Open the BlendView page in Editor Preferences.")),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")),
		FUIAction(FExecuteAction::CreateRaw(this, &FBlendViewModule::OpenPluginSettings)));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}
void FBlendViewModule::PopulateExperimentalFeaturesMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection(TEXT("BlendViewExperimentalFeatures"), FBlendViewLocalization::Text(TEXT("\u5B9E\u9A8C\u529F\u80FD"), TEXT("Experimental Features")));
	for (const FBlendViewFeatureToggleDefinition& Feature : FBlendViewFeatureSettings::GetExperimentalFeatures())
	{
		const FName PropertyName = Feature.PropertyName;
		MenuBuilder.AddMenuEntry(
			Feature.GetDisplayName(),
			Feature.GetTooltip(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, PropertyName]()
				{
					FBlendViewFeatureSettings::Toggle(GetMutableDefault<UBlendViewSettings>(), PropertyName);
					HandleExperimentalFeatureToggled(PropertyName);
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([PropertyName]()
				{
					return FBlendViewFeatureSettings::IsEnabled(GetDefault<UBlendViewSettings>(), PropertyName);
				})),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	}
	MenuBuilder.EndSection();
}
TSharedRef<SWidget> FBlendViewModule::GenerateTransformPivotToolbarMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	PopulateTransformPivotMenu(MenuBuilder);
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FBlendViewModule::GenerateSnapSettingsToolbarMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	PopulateSnapSettingsMenu(MenuBuilder);
	return MenuBuilder.MakeWidget();
}

void FBlendViewModule::PopulateTransformPivotMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u8FB9\u754C\u6846\u4E2D\u5FC3"), TEXT("Bounding Box Center")),
		FBlendViewLocalization::Text(
			TEXT("\u56F4\u7ED5\u6240\u6709\u9009\u4E2D\u5BF9\u8C61\u8054\u5408\u8FB9\u754C\u6846\u7684\u4E2D\u5FC3\u6574\u4F53\u65CB\u8F6C\u6216\u7F29\u653E\u3002"),
			TEXT("Rotate or scale the selection as a group around its combined bounding-box center.")),
		FSlateIcon(BlendViewStyleSetName, BlendViewPivotBoundingBoxCenterIconName),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::SetTransformPivotMode, EBlendViewTransformPivotMode::BoundingBoxCenter),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsTransformPivotMode, EBlendViewTransformPivotMode::BoundingBoxCenter)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("3D \u6E38\u6807"), TEXT("3D Cursor")),
		FBlendViewLocalization::Text(
			TEXT("\u56F4\u7ED5 BlendView 3D \u6E38\u6807\u4F4D\u7F6E\u65CB\u8F6C\u3001\u7F29\u653E\u6216\u955C\u50CF\u9009\u4E2D\u5BF9\u8C61\u3002"),
			TEXT("Rotate, scale, or mirror the selection around the BlendView 3D cursor.")),
		FSlateIcon(BlendViewStyleSetName, BlendViewPivotCursorIconName),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::SetTransformPivotMode, EBlendViewTransformPivotMode::Cursor),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsTransformPivotMode, EBlendViewTransformPivotMode::Cursor)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u5404\u81EA\u7684\u539F\u70B9"), TEXT("Individual Origins")),
		FBlendViewLocalization::Text(
			TEXT("\u6BCF\u4E2A\u9009\u4E2D\u5BF9\u8C61\u56F4\u7ED5\u81EA\u8EAB\u539F\u70B9\u72EC\u7ACB\u65CB\u8F6C\u6216\u7F29\u653E\u3002"),
			TEXT("Rotate or scale each selected object independently around its own origin.")),
		FSlateIcon(BlendViewStyleSetName, BlendViewPivotIndividualOriginsIconName),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::SetTransformPivotMode, EBlendViewTransformPivotMode::IndividualOrigins),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsTransformPivotMode, EBlendViewTransformPivotMode::IndividualOrigins)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u6D3B\u52A8\u5143\u7D20"), TEXT("Active Element")),
		FBlendViewLocalization::Text(
			TEXT("\u56F4\u7ED5\u6700\u540E\u9009\u4E2D\u7684\u6D3B\u52A8\u5143\u7D20\u539F\u70B9\u65CB\u8F6C\u3001\u7F29\u653E\u6216\u955C\u50CF\u9009\u4E2D\u5BF9\u8C61\u3002"),
			TEXT("Rotate, scale, or mirror the selection around the last selected active element.")),
		FSlateIcon(BlendViewStyleSetName, BlendViewPivotActiveItemIconName),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::SetTransformPivotMode, EBlendViewTransformPivotMode::ActiveItem),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsTransformPivotMode, EBlendViewTransformPivotMode::ActiveItem)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u4E16\u754C\u539F\u70B9"), TEXT("World Origin")),
		FBlendViewLocalization::Text(
			TEXT("\u56F4\u7ED5\u4E16\u754C\u5750\u6807\u539F\u70B9\u65CB\u8F6C\u3001\u7F29\u653E\u6216\u955C\u50CF\u9009\u4E2D\u5BF9\u8C61\u3002"),
			TEXT("Rotate, scale, or mirror the selection around the world origin.")),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("EditorViewport.RelativeCoordinateSystem_World")),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::SetTransformPivotMode, EBlendViewTransformPivotMode::WorldOrigin),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsTransformPivotMode, EBlendViewTransformPivotMode::WorldOrigin)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);
}
void FBlendViewModule::PopulateSnapSettingsMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection(TEXT("BlendViewSnapGeneral"), FBlendViewLocalization::Text(TEXT("\u901A\u7528"), TEXT("General")));
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("UE \u5438\u9644\u6355\u6349"), TEXT("UE Snap Capture")),
		FBlendViewLocalization::Text(TEXT("\u542F\u7528\u540E\uFF0CBlendView \u53D8\u6362\u4F1A\u8BFB\u53D6\u5F53\u524D UE \u7F51\u683C\u3001\u65CB\u8F6C\u548C\u7F29\u653E\u5438\u9644\u8BBE\u7F6E\u3002"), TEXT("Use the current UE grid, rotation, and scale snapping settings during BlendView transforms.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleUnrealEditorSnapCapture),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsUnrealEditorSnapCaptureEnabled)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u5438\u9644\u76EE\u6807\u8FB9\u7F18\u9650\u5236"), TEXT("Structural Edge Filter")),
		FBlendViewLocalization::Text(TEXT("\u5B9E\u9645\u5438\u9644\u76EE\u6807\u53EA\u4FDD\u7559\u7ED3\u6784\u9876\u70B9\u548C\u7ED3\u6784\u8FB9\uFF1B\u8BBE\u7F6E\u5438\u9644\u57FA\u51C6\u4ECD\u53EF\u9009\u62E9\u5B8C\u6574\u53EF\u89C1\u51E0\u4F55\u3002"), TEXT("Limit transform targets to structural vertices and edges; snap-base picking still uses all visible geometry.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleStructuralEdgeLimit),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsStructuralEdgeLimitEnabled)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u65CB\u8F6C\u5BF9\u9F50\u76EE\u6807"), TEXT("Align Rotation to Target")),
		FBlendViewLocalization::Text(
			TEXT("\u79FB\u52A8\u4E34\u65F6\u5438\u9644\u5230\u51E0\u4F55\u76EE\u6807\u65F6\uFF0C\u5C06\u5BF9\u8C61\u672C\u5730 Z \u8F74\u5BF9\u9F50\u5230\u5438\u9644\u76EE\u6807\u6CD5\u7EBF\u3002"),
			TEXT("Align each object's local Z axis to the snap target normal while temporary translation snapping to geometry.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleAlignRotationToSnapTarget),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsAlignRotationToSnapTargetEnabled)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("BlendViewSnapSource"), FBlendViewLocalization::Text(TEXT("\u5438\u9644\u57FA\u51C6"), TEXT("Snap Source")));
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u6700\u8FD1"), TEXT("Closest")),
		FBlendViewLocalization::Text(TEXT("\u627E\u5230\u5438\u9644\u76EE\u6807\u540E\uFF0C\u4F7F\u7528\u5F53\u524D\u9009\u4E2D\u9879\u4E2D\u79BB\u76EE\u6807\u6700\u8FD1\u7684\u9876\u70B9\u4F5C\u4E3A\u5438\u9644\u6E90\u3002"), TEXT("Use the selected vertex closest to the resolved target as the snap source.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::SetSnapSourceMode, EBlendViewSnapSourceMode::Closest),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsSnapSourceMode, EBlendViewSnapSourceMode::Closest)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u67A2\u8F74\u70B9"), TEXT("Pivot")),
		FBlendViewLocalization::Text(TEXT("\u4F7F\u7528\u5F53\u524D\u53D8\u6362\u67A2\u8F74\u70B9\u4F5C\u4E3A\u5438\u9644\u6E90\uFF0C\u5BF9\u5E94 Blender \u7684\u4E2D\u5FC3\u3002"), TEXT("Use the current transform pivot as the snap source, matching Blender's center.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::SetSnapSourceMode, EBlendViewSnapSourceMode::Pivot),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsSnapSourceMode, EBlendViewSnapSourceMode::Pivot)),
		NAME_None,
		EUserInterfaceActionType::RadioButton);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("BlendViewSnapTargets"), FBlendViewLocalization::Text(TEXT("\u5438\u9644\u76EE\u6807"), TEXT("Snap Targets")));
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u7F51\u683C"), TEXT("Grid")),
		FBlendViewLocalization::Text(TEXT("\u5141\u8BB8\u4E34\u65F6\u5438\u9644\u5230\u5F53\u524D\u7F51\u683C\u6B65\u8FDB\u70B9\u3002"), TEXT("Allow temporary snapping to the current grid increments.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleSnapTarget, EBlendViewSnapTargetKind::Grid),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsSnapTargetEnabled, EBlendViewSnapTargetKind::Grid)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u9876\u70B9"), TEXT("Vertex")),
		FBlendViewLocalization::Text(TEXT("\u5141\u8BB8\u5438\u9644\u5230\u53EF\u89C1\u9759\u6001\u7F51\u683C\u9876\u70B9\u3002"), TEXT("Allow snapping to visible static-mesh vertices.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleSnapTarget, EBlendViewSnapTargetKind::Vertex),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsSnapTargetEnabled, EBlendViewSnapTargetKind::Vertex)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u8FB9"), TEXT("Edge")),
		FBlendViewLocalization::Text(TEXT("\u5141\u8BB8\u5438\u9644\u5230\u53EF\u89C1\u9759\u6001\u7F51\u683C\u8FB9\u3002"), TEXT("Allow snapping to visible static-mesh edges.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleSnapTarget, EBlendViewSnapTargetKind::Edge),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsSnapTargetEnabled, EBlendViewSnapTargetKind::Edge)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u8FB9\u4E2D\u70B9"), TEXT("Edge Midpoint")),
		FBlendViewLocalization::Text(TEXT("\u5141\u8BB8\u5438\u9644\u5230\u53EF\u89C1\u9759\u6001\u7F51\u683C\u8FB9\u4E2D\u70B9\u3002"), TEXT("Allow snapping to visible static-mesh edge midpoints.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleSnapTarget, EBlendViewSnapTargetKind::EdgeMidpoint),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsSnapTargetEnabled, EBlendViewSnapTargetKind::EdgeMidpoint)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		FBlendViewLocalization::Text(TEXT("\u9762"), TEXT("Face")),
		FBlendViewLocalization::Text(TEXT("\u5141\u8BB8\u5438\u9644\u5230\u9F20\u6807\u5C04\u7EBF\u547D\u4E2D\u7684\u53EF\u89C1\u9759\u6001\u7F51\u683C\u9762\u3002"), TEXT("Allow snapping to visible static-mesh faces hit by the cursor ray.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBlendViewModule::ToggleSnapTarget, EBlendViewSnapTargetKind::Face),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FBlendViewModule::IsSnapTargetEnabled, EBlendViewSnapTargetKind::Face)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.EndSection();
}

FText FBlendViewModule::GetTransformPivotToolbarLabel() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings)
	{
		return FBlendViewLocalization::Text(TEXT("\u53D8\u6362\u8F74\u5FC3\u70B9"), TEXT("Pivot"));
	}

	switch (Settings->TransformPivotMode)
	{
	case EBlendViewTransformPivotMode::BoundingBoxCenter:
		return FBlendViewLocalization::Text(TEXT("\u8FB9\u754C\u6846\u4E2D\u5FC3"), TEXT("Bounding Box Center"));
	case EBlendViewTransformPivotMode::Cursor:
		return FBlendViewLocalization::Text(TEXT("3D \u6E38\u6807"), TEXT("3D Cursor"));
	case EBlendViewTransformPivotMode::IndividualOrigins:
		return FBlendViewLocalization::Text(TEXT("\u5404\u81EA\u7684\u539F\u70B9"), TEXT("Individual Origins"));
	case EBlendViewTransformPivotMode::ActiveItem:
		return FBlendViewLocalization::Text(TEXT("\u6D3B\u52A8\u5143\u7D20"), TEXT("Active Element"));
	case EBlendViewTransformPivotMode::WorldOrigin:
		return FBlendViewLocalization::Text(TEXT("\u4E16\u754C\u539F\u70B9"), TEXT("World Origin"));
	default:
		return FBlendViewLocalization::Text(TEXT("\u53D8\u6362\u8F74\u5FC3\u70B9"), TEXT("Pivot"));
	}
}

FText FBlendViewModule::GetTransformPivotToolbarTooltip() const
{
	return FBlendViewLocalization::Text(
		TEXT("\u9009\u62E9 BlendView \u65CB\u8F6C\u3001\u7F29\u653E\u548C\u955C\u50CF\u4F7F\u7528\u7684\u53D8\u6362\u8F74\u5FC3\u70B9\u3002"),
		TEXT("Choose the pivot used by BlendView rotation, scale, and mirror transforms."));
}

FSlateIcon FBlendViewModule::GetTransformPivotToolbarIcon() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings)
	{
		return FSlateIcon(BlendViewStyleSetName, BlendViewPivotBoundingBoxCenterIconName);
	}

	switch (Settings->TransformPivotMode)
	{
	case EBlendViewTransformPivotMode::BoundingBoxCenter:
		return FSlateIcon(BlendViewStyleSetName, BlendViewPivotBoundingBoxCenterIconName);
	case EBlendViewTransformPivotMode::Cursor:
		return FSlateIcon(BlendViewStyleSetName, BlendViewPivotCursorIconName);
	case EBlendViewTransformPivotMode::IndividualOrigins:
		return FSlateIcon(BlendViewStyleSetName, BlendViewPivotIndividualOriginsIconName);
	case EBlendViewTransformPivotMode::ActiveItem:
		return FSlateIcon(BlendViewStyleSetName, BlendViewPivotActiveItemIconName);
	case EBlendViewTransformPivotMode::WorldOrigin:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("EditorViewport.RelativeCoordinateSystem_World"));
	default:
		return FSlateIcon(BlendViewStyleSetName, BlendViewPivotBoundingBoxCenterIconName);
	}
}

FText FBlendViewModule::GetSnapSettingsToolbarLabel() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings)
	{
		return FBlendViewLocalization::Text(TEXT("\u65E0"), TEXT("None"));
	}

	const int32 EnabledTargetCount =
		(Settings->bSnapTargetGrid ? 1 : 0) +
		(Settings->bSnapTargetVertex ? 1 : 0) +
		(Settings->bSnapTargetEdge ? 1 : 0) +
		(Settings->bSnapTargetEdgeMidpoint ? 1 : 0) +
		(Settings->bSnapTargetFace ? 1 : 0);

	if (EnabledTargetCount == 0)
	{
		return FBlendViewLocalization::Text(TEXT("\u65E0"), TEXT("None"));
	}
	if (EnabledTargetCount > 1)
	{
		return FBlendViewLocalization::Text(TEXT("\u6DF7\u5408"), TEXT("Mixed"));
	}
	if (Settings->bSnapTargetGrid)
	{
		return FBlendViewLocalization::Text(TEXT("\u7F51\u683C"), TEXT("Grid"));
	}
	if (Settings->bSnapTargetVertex)
	{
		return FBlendViewLocalization::Text(TEXT("\u9876\u70B9"), TEXT("Vertex"));
	}
	if (Settings->bSnapTargetEdge)
	{
		return FBlendViewLocalization::Text(TEXT("\u8FB9"), TEXT("Edge"));
	}
	if (Settings->bSnapTargetEdgeMidpoint)
	{
		return FBlendViewLocalization::Text(TEXT("\u8FB9\u4E2D\u70B9"), TEXT("Midpoint"));
	}
	return FBlendViewLocalization::Text(TEXT("\u9762"), TEXT("Face"));
}

FText FBlendViewModule::GetSnapSettingsToolbarTooltip() const
{
	return FBlendViewLocalization::Text(
		TEXT("\u914D\u7F6E BlendView \u5438\u9644\u884C\u4E3A\u3001\u5438\u9644\u57FA\u51C6\u548C\u53EF\u7528\u5438\u9644\u76EE\u6807\u3002"),
		TEXT("Configure BlendView snapping behavior, snap source, and available snap targets."));
}

void FBlendViewModule::ToggleBlendViewEnabled()
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->SetBlendViewEnabled(!InputProcessor->IsBlendViewEnabled());
	}
}

bool FBlendViewModule::IsBlendViewEnabled() const
{
	return InputProcessor.IsValid() && InputProcessor->IsBlendViewEnabled();
}

void FBlendViewModule::ToggleTransformWorkflow()
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->bEnableTransformWorkflow = !Settings->bEnableTransformWorkflow;
	Settings->SaveConfig();
	if (!Settings->bEnableTransformWorkflow && InputProcessor.IsValid())
	{
		InputProcessor->CancelActiveOperation();
	}
}

bool FBlendViewModule::IsTransformWorkflowEnabled() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->bEnableTransformWorkflow;
}

void FBlendViewModule::ToggleMouseNavigation()
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->bEnableMouseNavigation = !Settings->bEnableMouseNavigation;
	Settings->SaveConfig();
	if (!Settings->bEnableMouseNavigation && InputProcessor.IsValid())
	{
		InputProcessor->CancelActiveOperation();
	}
}

bool FBlendViewModule::IsMouseNavigationEnabled() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->bEnableMouseNavigation;
}

void FBlendViewModule::ToggleCenterToolbar()
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->bShowCenterToolbar = !Settings->bShowCenterToolbar;
	Settings->SaveConfig();
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::Get()->RefreshAllWidgets();
	}
}

bool FBlendViewModule::IsCenterToolbarVisible() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->bShowCenterToolbar;
}

void FBlendViewModule::ToggleRightMouseFlyBoost()
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->bEnableShiftFlySpeedBoost = !Settings->bEnableShiftFlySpeedBoost;
	Settings->SaveConfig();
}

bool FBlendViewModule::IsRightMouseFlyBoostEnabled() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->bEnableShiftFlySpeedBoost;
}

void FBlendViewModule::HandleExperimentalFeatureToggled(const FName PropertyName)
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableSceneCursor) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnablePieMenu))
	{
		if (InputProcessor.IsValid())
		{
			InputProcessor->CancelActiveOperation();
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableSceneCursor) && GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}
void FBlendViewModule::TogglePersistentSnap()
{
	FBlendViewSessionState::TogglePersistentSnap();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FBlendViewModule::IsPersistentSnapEnabled() const
{
	return FBlendViewSessionState::IsPersistentSnapEnabled();
}

void FBlendViewModule::OpenPluginSettings() const
{
	if (ISettingsModule* SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>(TEXT("Settings")))
	{
		SettingsModule->ShowViewer(TEXT("Editor"), TEXT("Plugins"), TEXT("BlendView"));
	}
}

void FBlendViewModule::ToggleUnrealEditorSnapCapture()
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->bEnableUnrealEditorSnapCapture = !Settings->bEnableUnrealEditorSnapCapture;
	Settings->SaveConfig();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FBlendViewModule::IsUnrealEditorSnapCaptureEnabled() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->bEnableUnrealEditorSnapCapture;
}

void FBlendViewModule::ToggleStructuralEdgeLimit()
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->bEnableStructuralEdgeLimit = !Settings->bEnableStructuralEdgeLimit;
	Settings->SaveConfig();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FBlendViewModule::IsStructuralEdgeLimitEnabled() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->bEnableStructuralEdgeLimit;
}

void FBlendViewModule::ToggleAlignRotationToSnapTarget()
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->bAlignRotationToSnapTarget = !Settings->bAlignRotationToSnapTarget;
	Settings->SaveConfig();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FBlendViewModule::IsAlignRotationToSnapTargetEnabled() const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->bAlignRotationToSnapTarget;
}

void FBlendViewModule::SetSnapSourceMode(const EBlendViewSnapSourceMode Mode)
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->SnapSourceMode = Mode;
	Settings->SaveConfig();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FBlendViewModule::IsSnapSourceMode(const EBlendViewSnapSourceMode Mode) const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->SnapSourceMode == Mode;
}

void FBlendViewModule::SetTransformPivotMode(const EBlendViewTransformPivotMode Mode)
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	Settings->TransformPivotMode = Mode;
	Settings->SaveConfig();
}

bool FBlendViewModule::IsTransformPivotMode(const EBlendViewTransformPivotMode Mode) const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	return Settings && Settings->TransformPivotMode == Mode;
}

void FBlendViewModule::ToggleSnapTarget(const EBlendViewSnapTargetKind TargetKind)
{
	UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
	if (!Settings)
	{
		return;
	}

	switch (TargetKind)
	{
	case EBlendViewSnapTargetKind::Grid:
		Settings->bSnapTargetGrid = !Settings->bSnapTargetGrid;
		break;
	case EBlendViewSnapTargetKind::Vertex:
		Settings->bSnapTargetVertex = !Settings->bSnapTargetVertex;
		break;
	case EBlendViewSnapTargetKind::Edge:
		Settings->bSnapTargetEdge = !Settings->bSnapTargetEdge;
		break;
	case EBlendViewSnapTargetKind::EdgeMidpoint:
		Settings->bSnapTargetEdgeMidpoint = !Settings->bSnapTargetEdgeMidpoint;
		break;
	case EBlendViewSnapTargetKind::Face:
		Settings->bSnapTargetFace = !Settings->bSnapTargetFace;
		break;
	default:
		return;
	}

	Settings->SaveConfig();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FBlendViewModule::IsSnapTargetEnabled(const EBlendViewSnapTargetKind TargetKind) const
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	if (!Settings)
	{
		return false;
	}

	switch (TargetKind)
	{
	case EBlendViewSnapTargetKind::Grid:
		return Settings->bSnapTargetGrid;
	case EBlendViewSnapTargetKind::Vertex:
		return Settings->bSnapTargetVertex;
	case EBlendViewSnapTargetKind::Edge:
		return Settings->bSnapTargetEdge;
	case EBlendViewSnapTargetKind::EdgeMidpoint:
		return Settings->bSnapTargetEdgeMidpoint;
	case EBlendViewSnapTargetKind::Face:
		return Settings->bSnapTargetFace;
	default:
		return false;
	}
}

void FBlendViewModule::RegisterInputProcessor()
{
	if (bPreparedForEngineExit ||
		bInputProcessorRegistered ||
		!InputProcessor.IsValid() ||
		!FSlateApplication::IsInitialized())
	{
		return;
	}

	FSlateApplication::Get().RegisterInputPreProcessor(
		InputProcessor.ToSharedRef(),
		EInputPreProcessorType::PreEditor);
	bInputProcessorRegistered = true;
}

void FBlendViewModule::UnregisterInputProcessor()
{
	if (!bInputProcessorRegistered || !InputProcessor.IsValid() || !FSlateApplication::IsInitialized())
	{
		bInputProcessorRegistered = false;
		return;
	}

	FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor.ToSharedRef());
	bInputProcessorRegistered = false;
}

void FBlendViewModule::PrepareForEngineExit()
{
	if (bPreparedForEngineExit)
	{
		return;
	}

	bPreparedForEngineExit = true;
	if (ActiveSelectionTracker.IsValid())
	{
		ActiveSelectionTracker->Shutdown();
	}
	UnregisterInputProcessor();
	if (InputProcessor.IsValid())
	{
		InputProcessor->PrepareForEngineExit();
	}
	FBlendViewStatusBarPresenter::Get().PrepareForEngineExit();
}

void FBlendViewModule::OnBeginPIE(bool bIsSimulating)
{
	bWasRegisteredBeforePIE = bInputProcessorRegistered;
	if (InputProcessor.IsValid())
	{
		InputProcessor->CancelActiveOperation();
	}
	UnregisterInputProcessor();
}

void FBlendViewModule::OnEndPIE(bool bIsSimulating)
{
	if (bWasRegisteredBeforePIE && !bPreparedForEngineExit)
	{
		RegisterInputProcessor();
	}
	bWasRegisteredBeforePIE = false;
}

void FBlendViewModule::OnDebugDraw(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->DrawHUD(Canvas);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlendViewModule, BlendView)
