// Copyright 2026 RainskyCG. All Rights Reserved.

#include "QuickFavorites/BlendViewQuickFavorites.h"

#include "BlendViewCommands.h"
#include "BlendViewSettings.h"
#include "Compat/BlendViewEditorModeToolsCompat.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Editor.h"
#include "EdMode.h"
#include "EditorModeManager.h"
#include "InputCoreTypes.h"
#include "LevelEditor.h"
#include "Localization/BlendViewLocalization.h"
#include "Modules/ModuleManager.h"
#include "QuickFavorites/BlendViewQuickFavoriteCatalog.h"
#include "SLevelViewport.h"
#include "UI/BlendViewPopupStyle.h"
#include "UI/BlendViewShortcutBinding.h"
#include "UI/SBlendViewShortcutCapturePopup.h"
#include "Brushes/SlateNoResource.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Toolkits/BaseToolkit.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	using FBlendViewActionFavorite = BlendViewQuickFavoriteCatalog::FActionFavorite;
	using FBlendViewEditorModeFavorite = BlendViewQuickFavoriteCatalog::FEditorModeFavorite;
	using FBlendViewModeToolFavorite = BlendViewQuickFavoriteCatalog::FModeToolFavorite;
	using FBlendViewQuickFavoriteCommandItem = BlendViewQuickFavoriteCatalog::FCommandItem;

	using namespace BlendViewQuickFavoriteCatalog;

	constexpr float FavoriteMenuWidth = 320.0f;
	constexpr float CommandSearchMenuWidth = 520.0f;
	constexpr float MenuMaxHeight = 460.0f;

	enum class EBlendViewQuickMenuMode : uint8
	{
		Favorites,
		CommandSearch
	};

	const FScrollBoxStyle* GetMenuScrollBoxStyle()
	{
		static const FScrollBoxStyle Style = []()
		{
			FScrollBoxStyle NewStyle = FAppStyle::Get().GetWidgetStyle<FScrollBoxStyle>(TEXT("ScrollBox"));
			static const FSlateNoResource NoShadow;
			NewStyle.SetTopShadowBrush(NoShadow);
			NewStyle.SetBottomShadowBrush(NoShadow);
			return NewStyle;
		}();
		return &Style;
	}

	bool IsQuickFavoritesTextEntryFocused()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		const TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
		if (!FocusedWidget.IsValid())
		{
			return false;
		}

		const FString WidgetType = FocusedWidget->GetType().ToString();
		return WidgetType.Contains(TEXT("EditableText")) ||
			WidgetType.Contains(TEXT("SearchBox")) ||
			WidgetType.Contains(TEXT("SuggestionTextBox"));
	}

	bool MatchesCommandChord(const FKeyEvent& KeyEvent, const TSharedPtr<FUICommandInfo>& Command)
	{
		if (!Command.IsValid())
		{
			return false;
		}

		const FInputChord& Chord = *Command->GetActiveChord(EMultipleKeyBindingIndex::Primary);
		return Chord.IsValidChord() &&
			!KeyEvent.IsRepeat() &&
			KeyEvent.GetKey() == Chord.Key &&
			KeyEvent.IsShiftDown() == Chord.bShift &&
			KeyEvent.IsControlDown() == Chord.NeedsControl() &&
			KeyEvent.IsAltDown() == Chord.NeedsAlt() &&
			KeyEvent.IsCommandDown() == Chord.NeedsCommand();
	}

	bool IsQuickFavoritesChord(const FKeyEvent& KeyEvent)
	{
		return FBlendViewCommands::IsRegistered() &&
			MatchesCommandChord(KeyEvent, FBlendViewCommands::Get().OpenQuickFavorites);
	}

	bool IsCommandSearchChord(const FKeyEvent& KeyEvent)
	{
		return FBlendViewCommands::IsRegistered() &&
			MatchesCommandChord(KeyEvent, FBlendViewCommands::Get().OpenCommandSearch);
	}

	EBlendViewQuickFavoriteContext DetectContextUnderCursor(FSlateApplication& SlateApp, TSharedPtr<SLevelViewport>& OutLevelViewport)
	{
		const FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(
			SlateApp.GetCursorPos(),
			SlateApp.GetInteractiveTopLevelWindows(),
			false);
		if (!WidgetPath.IsValid())
		{
			return EBlendViewQuickFavoriteContext::Global;
		}

		for (int32 Index = 0; Index < WidgetPath.Widgets.Num(); ++Index)
		{
			const FArrangedWidget& ArrangedWidget = WidgetPath.Widgets[Index];
			const FName WidgetTypeName = ArrangedWidget.Widget->GetType();
			if (WidgetTypeName == TEXT("SLevelViewport"))
			{
				OutLevelViewport = StaticCastSharedRef<SLevelViewport>(ArrangedWidget.Widget);
				return EBlendViewQuickFavoriteContext::LevelViewport;
			}

			const FString WidgetType = WidgetTypeName.ToString();
			if (WidgetType.Contains(TEXT("Graph")) || WidgetType.Contains(TEXT("NodePanel")))
			{
				return EBlendViewQuickFavoriteContext::GraphEditor;
			}
			if (WidgetType.Contains(TEXT("ContentBrowser")) ||
				WidgetType.Contains(TEXT("AssetView")) ||
				WidgetType.Contains(TEXT("PathView")))
			{
				return EBlendViewQuickFavoriteContext::ContentBrowser;
			}
		}

		return EBlendViewQuickFavoriteContext::Global;
	}

	class SBlendViewQuickFavoritesMenu final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlendViewQuickFavoritesMenu) {}
			SLATE_ARGUMENT(EBlendViewQuickFavoriteContext, ActiveContext)
			SLATE_ARGUMENT(TWeakPtr<SLevelViewport>, LevelViewport)
			SLATE_ARGUMENT(FBlendViewQuickFavoriteCommandListMap*, CommandListsByContext)
			SLATE_ARGUMENT(EBlendViewQuickMenuMode, MenuMode)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			ActiveContext = InArgs._ActiveContext;
			LevelViewport = InArgs._LevelViewport;
			CommandListsByContext = InArgs._CommandListsByContext;
			MenuMode = InArgs._MenuMode;
			GatherAllCommandItems(AllCommandItems, ActiveContext);

			ChildSlot
			[
				BlendViewPopupStyle::MakeMenuShell(
					SNew(SBox)
					.WidthOverride(this, &SBlendViewQuickFavoritesMenu::GetMenuWidth)
					.MaxDesiredHeight(MenuMaxHeight)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(4.0f, 2.0f, 4.0f, 6.0f))
						[
							SNew(SBox)
							.Visibility(this, &SBlendViewQuickFavoritesMenu::GetFavoritesHeaderVisibility)
							[
								SNew(STextBlock)
								.Text(this, &SBlendViewQuickFavoritesMenu::GetTitleText)
								.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
						[
							SNew(SBox)
							.Visibility(this, &SBlendViewQuickFavoritesMenu::GetFavoritesHeaderVisibility)
							.HeightOverride(1.0f)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush(TEXT("Menu.Separator")))
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
						[
							SAssignNew(SearchBoxContainer, SBox)
							.Visibility(this, &SBlendViewQuickFavoritesMenu::GetSearchBoxVisibility)
							[
								SAssignNew(SearchBox, SSearchBox)
								.HintText(FBlendViewLocalization::Text(TEXT("搜索命令..."), TEXT("Search commands...")))
								.OnTextChanged(this, &SBlendViewQuickFavoritesMenu::OnSearchChanged)
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SAssignNew(ListBox, SScrollBox)
							.Style(GetMenuScrollBoxStyle())
						]
					],
					FMargin(8.0f, 7.0f))
			];

			RefreshList();
			if (IsCommandSearchMode())
			{
				RegisterActiveTimer(
					0.0f,
					FWidgetActiveTimerDelegate::CreateSP(this, &SBlendViewQuickFavoritesMenu::FocusSearchBox));
			}
		}

	private:
		EActiveTimerReturnType FocusSearchBox(double, float)
		{
			if (SearchBox.IsValid() && FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
			}
			return EActiveTimerReturnType::Stop;
		}

		FText GetTitleText() const
		{
			return FBlendViewLocalization::Text(TEXT("快速收藏夹"), TEXT("Quick Favorites"));
		}

		EVisibility GetFavoritesHeaderVisibility() const
		{
			return IsCommandSearchMode() ? EVisibility::Collapsed : EVisibility::Visible;
		}

		EVisibility GetSearchBoxVisibility() const
		{
			return IsCommandSearchMode() ? EVisibility::Visible : EVisibility::Collapsed;
		}

		FOptionalSize GetMenuWidth() const
		{
			return IsCommandSearchMode() ? CommandSearchMenuWidth : FavoriteMenuWidth;
		}

		void OnSearchChanged(const FText& NewText)
		{
			SearchText = NewText.ToString();
			RefreshList();
		}

		bool IsCommandSearchMode() const
		{
			return MenuMode == EBlendViewQuickMenuMode::CommandSearch;
		}

		void RefreshList()
		{
			if (!ListBox.IsValid())
			{
				return;
			}

			ListBox->ClearChildren();
			if (IsCommandSearchMode())
			{
				BuildCommandSearchList();
			}
			else
			{
				BuildFavoritesList();
			}
		}

		void RefreshCommandCatalogAndList()
		{
			AllCommandItems.Reset();
			GatherAllCommandItems(AllCommandItems, ActiveContext);
			RefreshList();
		}

		void BuildFavoritesList()
		{
			const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
			if (!Settings)
			{
				return;
			}

			int32 VisibleCount = 0;
			for (int32 Index = 0; Index < Settings->QuickFavorites.Num(); ++Index)
			{
				const FBlendViewQuickFavoriteCommand& Favorite = Settings->QuickFavorites[Index];
				if (!AllowsContext(Favorite, ActiveContext))
				{
					continue;
				}

				FBlendViewQuickFavoriteCommandItem Item;
				if (!TryMakeFavoriteItem(Favorite, Item) || !CanExecuteItem(Item))
				{
					continue;
				}

				AddFavoriteRow(Item, Index);
				++VisibleCount;
			}

			if (VisibleCount == 0)
			{
				AddEmptyText(FBlendViewLocalization::Text(TEXT("没有可用收藏"), TEXT("No available favorites")));
			}
		}

		void BuildCommandSearchList()
		{
			const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
			if (!Settings)
			{
				return;
			}

			int32 VisibleCount = 0;
			for (const FBlendViewQuickFavoriteCommandItem& Item : AllCommandItems)
			{
				if (!MatchesSearch(Item, SearchText))
				{
					continue;
				}

				if (!CanExecuteItem(Item))
				{
					continue;
				}

				AddSearchCommandRow(Item);
				if (++VisibleCount >= 120)
				{
					break;
				}
			}

			if (VisibleCount == 0)
			{
				AddEmptyText(FBlendViewLocalization::Text(TEXT("没有可用命令"), TEXT("No available commands for this context")));
			}
		}

		void AddEmptyText(const FText& Text)
		{
			ListBox->AddSlot()
			.Padding(FMargin(8.0f, 10.0f))
			[
				SNew(STextBlock)
				.Text(Text)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}

		void AddFavoriteRow(const FBlendViewQuickFavoriteCommandItem& Item, const int32 FavoriteIndex)
		{
			ListBox->AddSlot()
			.Padding(FMargin(0.0f, 1.0f))
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))
				.OnMouseButtonDown(this, &SBlendViewQuickFavoritesMenu::OnFavoriteRowMouseDown, FavoriteIndex)
				[
					MakeCommandButton(Item, FOnClicked::CreateSP(this, &SBlendViewQuickFavoritesMenu::OnExecuteCommand, Item), false)
				]
			];
		}

		void AddSearchCommandRow(const FBlendViewQuickFavoriteCommandItem& Item)
		{
			ListBox->AddSlot()
			.Padding(FMargin(0.0f, 1.0f))
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))
				.OnMouseButtonDown(this, &SBlendViewQuickFavoritesMenu::OnSearchRowMouseDown, Item)
				[
					MakeCommandButton(Item, FOnClicked::CreateSP(this, &SBlendViewQuickFavoritesMenu::OnExecuteCommand, Item), true)
				]
			];
		}

		bool TryMakeFavoriteItem(
			const FBlendViewQuickFavoriteCommand& Favorite,
			FBlendViewQuickFavoriteCommandItem& OutItem) const
		{
			const FName StableId = GetFavoriteStableId(Favorite);
			switch (Favorite.SourceType)
			{
			case EBlendViewQuickFavoriteSource::NativeCommand:
				if (const TSharedPtr<FUICommandInfo> CommandInfo =
					FindCommand(Favorite.BindingContext, Favorite.CommandName))
				{
					OutItem = MakeNativeCommandItem(CommandInfo.ToSharedRef());
					return true;
				}
				break;
			case EBlendViewQuickFavoriteSource::EditorMode:
				if (const TOptional<FBlendViewEditorModeFavorite> EditorMode = FindEditorModeFavorite(StableId))
				{
					OutItem = MakeEditorModeItem(EditorMode.GetValue());
					return true;
				}
				break;
			case EBlendViewQuickFavoriteSource::ModeTool:
				if (const TOptional<FBlendViewModeToolFavorite> ModeTool = FindModeToolFavorite(StableId))
				{
					OutItem = MakeModeToolItem(ModeTool.GetValue());
					return true;
				}
				break;
			case EBlendViewQuickFavoriteSource::BlendViewAction:
				if (const TOptional<FBlendViewActionFavorite> Action = FindBlendViewActionFavorite(StableId))
				{
					OutItem = MakeBlendViewActionItem(Action.GetValue());
					return true;
				}
				break;
			default:
				break;
			}
			return false;
		}

		bool CanExecuteItem(const FBlendViewQuickFavoriteCommandItem& Item) const
		{
			switch (Item.SourceType)
			{
			case EBlendViewQuickFavoriteSource::NativeCommand:
				if (const TSharedPtr<FUICommandInfo> CommandInfo = FindCommand(Item.BindingContext, Item.CommandName))
				{
					return FindExecutableCommandList(CommandInfo.ToSharedRef()).IsValid();
				}
				return false;
			case EBlendViewQuickFavoriteSource::EditorMode:
				if (const TOptional<FBlendViewEditorModeFavorite> EditorMode = FindEditorModeFavorite(Item.StableId))
				{
					return ActiveContext == EBlendViewQuickFavoriteContext::LevelViewport &&
						BlendViewEditorModeToolsCompat::IsLevelEditorModeToolsAvailable() &&
						IsEditorModeVisibleInFavorites(*EditorMode);
				}
				return false;
			case EBlendViewQuickFavoriteSource::ModeTool:
				if (const TOptional<FBlendViewModeToolFavorite> ModeTool = FindModeToolFavorite(Item.StableId))
				{
					return ActiveContext == EBlendViewQuickFavoriteContext::LevelViewport &&
						BlendViewEditorModeToolsCompat::IsLevelEditorModeToolsAvailable() &&
						IsModuleAvailable(ModeTool->RequiredModuleName);
				}
				return false;
			case EBlendViewQuickFavoriteSource::BlendViewAction:
				return ActiveContext == EBlendViewQuickFavoriteContext::LevelViewport &&
					FindBlendViewActionFavorite(Item.StableId).IsSet();
			default:
				return false;
			}
		}

		TSharedRef<SWidget> MakeCommandButton(
			const FBlendViewQuickFavoriteCommandItem& Item,
			const FOnClicked& OnClicked,
			const bool bShowContext = false)
		{
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("Menu.Button"))
				.ContentPadding(FMargin(7.0f, 4.0f))
				.OnClicked(OnClicked)
				.ToolTipText(Item.Description)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(Item.Label)
							.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(Item.ContextLabel)
							.TextStyle(FAppStyle::Get(), TEXT("SmallText"))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Visibility(bShowContext ? EVisibility::Visible : EVisibility::Collapsed)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(Item.InputText)
						.TextStyle(FAppStyle::Get(), TEXT("SmallText"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				];
		}

		FReply OnFavoriteRowMouseDown(const FGeometry&, const FPointerEvent& MouseEvent, const int32 FavoriteIndex)
		{
			if (MouseEvent.GetEffectingButton() != EKeys::RightMouseButton || !FSlateApplication::IsInitialized())
			{
				return FReply::Unhandled();
			}

			FMenuBuilder MenuBuilder(true, nullptr);
			MenuBuilder.AddMenuEntry(
				FBlendViewLocalization::Text(TEXT("从快速收藏夹移除"), TEXT("Remove from Quick Favorites")),
				FBlendViewLocalization::Text(TEXT("移除此命令收藏。"), TEXT("Remove this command from quick favorites.")),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Delete")),
				FUIAction(FExecuteAction::CreateSP(this, &SBlendViewQuickFavoritesMenu::RemoveFavoriteByIndex, FavoriteIndex)));

			FSlateApplication::Get().PushMenu(
				AsShared(),
				FWidgetPath(),
				MakeStyledSubMenu(MenuBuilder.MakeWidget()),
				MouseEvent.GetScreenSpacePosition(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu),
				true);
			return FReply::Handled();
		}

		FReply OnSearchRowMouseDown(
			const FGeometry&,
			const FPointerEvent& MouseEvent,
			const FBlendViewQuickFavoriteCommandItem Item)
		{
			if (MouseEvent.GetEffectingButton() != EKeys::RightMouseButton || !FSlateApplication::IsInitialized())
			{
				return FReply::Unhandled();
			}

			const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
			const bool bAlreadyFavorite =
				Settings && IsDuplicateFavorite(*Settings, Item.SourceType, Item.StableId, ActiveContext);

			FMenuBuilder MenuBuilder(false, nullptr);
			if (bAlreadyFavorite)
			{
				MenuBuilder.AddMenuEntry(
					FBlendViewLocalization::Text(TEXT("从快速收藏夹移除"), TEXT("Remove from Quick Favorites")),
					FBlendViewLocalization::Text(TEXT("移除此命令收藏。"), TEXT("Remove this command from quick favorites.")),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Delete")),
					FUIAction(FExecuteAction::CreateSP(this, &SBlendViewQuickFavoritesMenu::RemoveFavoriteByItem, Item)));
			}
			else
			{
				MenuBuilder.AddMenuEntry(
					FBlendViewLocalization::Text(TEXT("添加到快速收藏夹"), TEXT("Add to Quick Favorites")),
					FBlendViewLocalization::Text(TEXT("将此命令添加到快速收藏夹。"), TEXT("Add this command to quick favorites.")),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Plus")),
					FUIAction(FExecuteAction::CreateSP(this, &SBlendViewQuickFavoritesMenu::AddFavoriteItem, Item)));
			}

			if (const TSharedPtr<FUICommandInfo> ShortcutCommand = FindShortcutCommand(Item))
			{
				MenuBuilder.AddMenuSeparator();
				const bool bHasCustomShortcut = BlendViewShortcutBinding::HasCustomPrimaryShortcut(ShortcutCommand);
				if (bHasCustomShortcut)
				{
					MenuBuilder.AddMenuEntry(
						FBlendViewLocalization::Text(TEXT("改变快捷键"), TEXT("Change Shortcut")),
						FBlendViewLocalization::Text(TEXT("为此命令设置新的主快捷键。"), TEXT("Set a new primary shortcut for this command.")),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Edit")),
						FUIAction(FExecuteAction::CreateSP(this, &SBlendViewQuickFavoritesMenu::OpenShortcutCapturePopup, ShortcutCommand, true)));
					MenuBuilder.AddMenuEntry(
						FBlendViewLocalization::Text(TEXT("移除快捷键"), TEXT("Remove Shortcut")),
						FBlendViewLocalization::Text(TEXT("移除此命令的自定义快捷键并恢复默认。"), TEXT("Remove the custom shortcut and restore the default.")),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Delete")),
						FUIAction(FExecuteAction::CreateSP(this, &SBlendViewQuickFavoritesMenu::ResetCommandShortcut, ShortcutCommand)));
				}
				else
				{
					MenuBuilder.AddMenuEntry(
						FBlendViewLocalization::Text(TEXT("指定快捷键"), TEXT("Assign Shortcut")),
						FBlendViewLocalization::Text(TEXT("为此命令指定主快捷键。"), TEXT("Assign a primary shortcut to this command.")),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Edit")),
						FUIAction(FExecuteAction::CreateSP(this, &SBlendViewQuickFavoritesMenu::OpenShortcutCapturePopup, ShortcutCommand, false)));
				}
			}

			FSlateApplication::Get().PushMenu(
				AsShared(),
				FWidgetPath(),
				MakeStyledSubMenu(MenuBuilder.MakeWidget()),
				MouseEvent.GetScreenSpacePosition(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu),
				true);
			return FReply::Handled();
		}

		void OpenShortcutCapturePopup(
			const TSharedPtr<FUICommandInfo> CommandInfo,
			const bool bChangingExistingShortcut)
		{
			if (!CommandInfo.IsValid() || !FSlateApplication::IsInitialized())
			{
				return;
			}

			FSlateApplication::Get().PushMenu(
				AsShared(),
				FWidgetPath(),
				MakeStyledSubMenu(
					SNew(SBlendViewShortcutCapturePopup)
					.CommandInfo(CommandInfo)
					.bChangingExistingShortcut(bChangingExistingShortcut)
					.OnShortcutChanged(FSimpleDelegate::CreateSP(this, &SBlendViewQuickFavoritesMenu::RefreshCommandCatalogAndList))),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu),
				true,
				FVector2D::ZeroVector,
				EPopupMethod::UseCurrentWindow);
		}

		void ResetCommandShortcut(const TSharedPtr<FUICommandInfo> CommandInfo)
		{
			if (!CommandInfo.IsValid())
			{
				return;
			}

			BlendViewShortcutBinding::ResetPrimaryShortcut(CommandInfo.ToSharedRef());
			RefreshCommandCatalogAndList();
		}

		TSharedRef<SWidget> MakeStyledSubMenu(const TSharedRef<SWidget>& MenuContent) const
		{
			return BlendViewPopupStyle::MakeFloatingMenuShell(MenuContent);
		}

		bool ExecuteModeTool(const FBlendViewModeToolFavorite& ModeTool) const
		{
			if (!BlendViewEditorModeToolsCompat::IsLevelEditorModeToolsAvailable() || !IsModuleAvailable(ModeTool.RequiredModuleName))
			{
				return false;
			}

			if (!ModeTool.RequiredModuleName.IsNone())
			{
				FModuleManager::Get().LoadModule(ModeTool.RequiredModuleName);
			}

			if (const TOptional<FBlendViewEditorModeFavorite> ParentMode =
				FindEditorModeFavorite(ModeTool.ParentModeStableId))
			{
				if (!EnsureEditorModeRegistered(*ParentMode))
				{
					return false;
				}
			}

			GLevelEditorModeTools().ActivateMode(ModeTool.ParentModeId);

			const TSharedPtr<FUICommandInfo> CommandInfo =
				FindCommand(ModeTool.BindingContext, ModeTool.CommandName);
			if (!CommandInfo.IsValid())
			{
				return false;
			}

			if (FEdMode* ActiveMode = GLevelEditorModeTools().GetActiveMode(ModeTool.ParentModeId))
			{
				if (const TSharedPtr<FModeToolkit> Toolkit = ActiveMode->GetToolkit())
				{
					const TSharedRef<FUICommandList> ToolkitCommands = Toolkit->GetToolkitCommands();
					if (ToolkitCommands->IsActionMapped(CommandInfo.ToSharedRef()) &&
						ToolkitCommands->CanExecuteAction(CommandInfo.ToSharedRef()))
					{
						ToolkitCommands->TryExecuteAction(CommandInfo.ToSharedRef());
						return true;
					}
				}
			}

			if (const TSharedPtr<FUICommandList> CommandList = FindExecutableCommandList(CommandInfo.ToSharedRef()))
			{
				CommandList->TryExecuteAction(CommandInfo.ToSharedRef());
				return true;
			}

			return false;
		}

		FReply OnExecuteCommand(FBlendViewQuickFavoriteCommandItem Item)
		{
			switch (Item.SourceType)
			{
			case EBlendViewQuickFavoriteSource::NativeCommand:
				if (const TSharedPtr<FUICommandInfo> CommandInfo = FindCommand(Item.BindingContext, Item.CommandName))
				{
					if (const TSharedPtr<FUICommandList> CommandList = FindExecutableCommandList(CommandInfo.ToSharedRef()))
					{
						CommandList->TryExecuteAction(CommandInfo.ToSharedRef());
					}
				}
				break;
			case EBlendViewQuickFavoriteSource::EditorMode:
				if (const TOptional<FBlendViewEditorModeFavorite> EditorMode = FindEditorModeFavorite(Item.StableId))
				{
					if (BlendViewEditorModeToolsCompat::IsLevelEditorModeToolsAvailable() && EnsureEditorModeRegistered(*EditorMode))
					{
						GLevelEditorModeTools().ActivateMode(EditorMode->ModeId);
					}
				}
				break;
			case EBlendViewQuickFavoriteSource::ModeTool:
				if (const TOptional<FBlendViewModeToolFavorite> ModeTool = FindModeToolFavorite(Item.StableId))
				{
					ExecuteModeTool(ModeTool.GetValue());
				}
				break;
			case EBlendViewQuickFavoriteSource::BlendViewAction:
				ExecuteBlendViewAction(Item.StableId);
				break;
			default:
				break;
			}

			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().DismissAllMenus();
			}
			return FReply::Handled();
		}

		void AddFavoriteItem(FBlendViewQuickFavoriteCommandItem Item)
		{
			UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
			if (!Settings || IsDuplicateFavorite(*Settings, Item.SourceType, Item.StableId, ActiveContext))
			{
				return;
			}

			FBlendViewQuickFavoriteCommand& Favorite = Settings->QuickFavorites.AddDefaulted_GetRef();
			Favorite.SourceType = Item.SourceType;
			Favorite.StableId = Item.StableId;
			Favorite.BindingContext = Item.BindingContext;
			Favorite.CommandName = Item.CommandName;
			Favorite.AllowedContexts.Add(ActiveContext);
			Settings->SaveConfig();

			RefreshList();
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().DismissAllMenus();
			}
		}

		void RemoveFavoriteByItem(FBlendViewQuickFavoriteCommandItem Item)
		{
			UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
			if (!Settings)
			{
				return;
			}

			for (int32 Index = 0; Index < Settings->QuickFavorites.Num(); ++Index)
			{
				const FBlendViewQuickFavoriteCommand& Favorite = Settings->QuickFavorites[Index];
				if (Favorite.SourceType == Item.SourceType &&
					GetFavoriteStableId(Favorite) == Item.StableId &&
					AllowsContext(Favorite, ActiveContext))
				{
					Settings->QuickFavorites.RemoveAt(Index);
					Settings->SaveConfig();
					RefreshList();
					if (FSlateApplication::IsInitialized())
					{
						FSlateApplication::Get().DismissAllMenus();
					}
					return;
				}
			}
		}

		void RemoveFavoriteByIndex(const int32 FavoriteIndex)
		{
			UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
			if (Settings && Settings->QuickFavorites.IsValidIndex(FavoriteIndex))
			{
				Settings->QuickFavorites.RemoveAt(FavoriteIndex);
				Settings->SaveConfig();
				RefreshList();
			}
		}

		TSharedPtr<FUICommandList> FindExecutableCommandList(const TSharedRef<FUICommandInfo>& CommandInfo) const
		{
			auto CanUseCommandList = [&CommandInfo](const TSharedPtr<FUICommandList>& CommandList)
			{
				return CommandList.IsValid() &&
					CommandList->IsActionMapped(CommandInfo) &&
					CommandList->CanExecuteAction(CommandInfo);
			};

			if (const TSharedPtr<SLevelViewport> PinnedViewport = LevelViewport.Pin())
			{
				if (CanUseCommandList(PinnedViewport->GetCommandList()))
				{
					return PinnedViewport->GetCommandList();
				}
			}

			if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
			{
				const FLevelEditorModule& LevelEditorModule =
					FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
				const TSharedRef<FUICommandList> LevelEditorActions =
					LevelEditorModule.GetGlobalLevelEditorActions();
				if (CanUseCommandList(LevelEditorActions))
				{
					return LevelEditorActions;
				}
			}

			if (CommandListsByContext)
			{
				if (const TArray<TWeakPtr<FUICommandList>>* Lists =
					CommandListsByContext->Find(CommandInfo->GetBindingContext()))
				{
					for (const TWeakPtr<FUICommandList>& WeakList : *Lists)
					{
						if (TSharedPtr<FUICommandList> CommandList = WeakList.Pin())
						{
							if (CanUseCommandList(CommandList))
							{
								return CommandList;
							}
						}
					}
				}
			}

			return nullptr;
		}

		EBlendViewQuickFavoriteContext ActiveContext = EBlendViewQuickFavoriteContext::Global;
		TWeakPtr<SLevelViewport> LevelViewport;
		FBlendViewQuickFavoriteCommandListMap* CommandListsByContext = nullptr;
		FString SearchText;
		EBlendViewQuickMenuMode MenuMode = EBlendViewQuickMenuMode::Favorites;
		TArray<FBlendViewQuickFavoriteCommandItem> AllCommandItems;
		TSharedPtr<SSearchBox> SearchBox;
		TSharedPtr<SBox> SearchBoxContainer;
		TSharedPtr<SScrollBox> ListBox;
	};
}

FBlendViewQuickFavorites::FBlendViewQuickFavorites()
{
	RegisterCommandListHandle = FInputBindingManager::Get().OnRegisterCommandList.AddRaw(
		this,
		&FBlendViewQuickFavorites::HandleRegisterCommandList);
	UnregisterCommandListHandle = FInputBindingManager::Get().OnUnregisterCommandList.AddRaw(
		this,
		&FBlendViewQuickFavorites::HandleUnregisterCommandList);
}

FBlendViewQuickFavorites::~FBlendViewQuickFavorites()
{
	FInputBindingManager::Get().OnRegisterCommandList.Remove(RegisterCommandListHandle);
	FInputBindingManager::Get().OnUnregisterCommandList.Remove(UnregisterCommandListHandle);
}

bool FBlendViewQuickFavorites::TryOpen(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent)
{
	if (!IsQuickFavoritesChord(KeyEvent))
	{
		return false;
	}
	return TryOpenMenu(SlateApp, KeyEvent, false);
}

bool FBlendViewQuickFavorites::TryOpenSearch(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent)
{
	if (!IsCommandSearchChord(KeyEvent))
	{
		return false;
	}
	return TryOpenMenu(SlateApp, KeyEvent, true);
}

bool FBlendViewQuickFavorites::TryOpenMenu(
	FSlateApplication& SlateApp,
	const FKeyEvent& KeyEvent,
	const bool bCommandSearch)
{
	const UBlendViewSettings* Settings = GetDefault<UBlendViewSettings>();
	const bool bFeatureEnabled = bCommandSearch
		? Settings && Settings->bEnableCommandSearch
		: Settings && Settings->bEnableQuickFavorites;
	if (!Settings ||
		!bFeatureEnabled ||
		IsQuickFavoritesTextEntryFocused() ||
		SlateApp.GetPressedMouseButtons().Contains(EKeys::RightMouseButton))
	{
		return false;
	}

	TSharedPtr<SLevelViewport> LevelViewport;
	const EBlendViewQuickFavoriteContext ActiveContext =
		DetectContextUnderCursor(SlateApp, LevelViewport);

	TSharedPtr<SWidget> ParentWidget = SlateApp.GetActiveTopLevelWindow();
	if (!ParentWidget.IsValid())
	{
		return false;
	}

	SlateApp.DismissAllMenus();
	SlateApp.PushMenu(
		ParentWidget.ToSharedRef(),
		FWidgetPath(),
		SNew(SBlendViewQuickFavoritesMenu)
			.ActiveContext(ActiveContext)
			.LevelViewport(LevelViewport)
			.CommandListsByContext(&CommandListsByContext)
			.MenuMode(bCommandSearch ? EBlendViewQuickMenuMode::CommandSearch : EBlendViewQuickMenuMode::Favorites),
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu),
		true,
		FVector2D::ZeroVector,
		EPopupMethod::UseCurrentWindow);
	return true;
}

void FBlendViewQuickFavorites::HandleRegisterCommandList(
	const FName BindingContext,
	TSharedRef<FUICommandList> CommandList)
{
	TArray<TWeakPtr<FUICommandList>>& Lists = CommandListsByContext.FindOrAdd(BindingContext);
	Lists.RemoveAll([](const TWeakPtr<FUICommandList>& ExistingList)
	{
		return !ExistingList.IsValid();
	});
	Lists.AddUnique(CommandList);
}

void FBlendViewQuickFavorites::HandleUnregisterCommandList(
	const FName BindingContext,
	TSharedRef<FUICommandList> CommandList)
{
	if (TArray<TWeakPtr<FUICommandList>>* Lists = CommandListsByContext.Find(BindingContext))
	{
		Lists->RemoveAll([&CommandList](const TWeakPtr<FUICommandList>& ExistingList)
		{
			return !ExistingList.IsValid() || ExistingList.Pin() == CommandList;
		});
		if (Lists->IsEmpty())
		{
			CommandListsByContext.Remove(BindingContext);
		}
	}
}
