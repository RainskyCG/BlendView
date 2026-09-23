// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewMoveToFolderMenu.h"

#include "Editor.h"
#include "EditorActorFolders.h"
#include "EditorSupportDelegates.h"
#include "Folder.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "Localization/BlendViewLocalization.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Styling/AppStyle.h"
#include "UI/BlendViewPopupStyle.h"
#include "Viewport/BlendViewViewportContext.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FName FolderIconBrush(TEXT("Icons.FolderClosed"));
	const FName LevelIconBrush(TEXT("LevelEditor.Tabs.Levels"));
	const FName NewFolderIconBrush(TEXT("Icons.Plus"));
	const FName SubMenuArrowBrush(TEXT("Icons.ChevronRight"));
	const FVector2D NewFolderMenuSize(260.0f, 138.0f);

	TSharedRef<SWidget> MakeSeparator()
	{
		return BlendViewPopupStyle::MakeSeparator();
	}

	TSharedRef<SWidget> MakeMenuContentShell(const TSharedRef<SWidget>& Content)
	{
		return BlendViewPopupStyle::MakeMenuContentShell(Content);
	}

	TSharedRef<SWidget> MakeMenuShell(const TSharedRef<SWidget>& Content)
	{
		return BlendViewPopupStyle::MakeMenuShell(Content);
	}

	struct FBlendViewFolderNode
	{
		FName Path;
		FText Label;
		FString SearchText;
		TArray<TSharedPtr<FBlendViewFolderNode>> Children;
	};

	DECLARE_DELEGATE_OneParam(FBlendViewMoveToFolderDelegate, FName);
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FBlendViewBuildSubMenuDelegate, TSharedPtr<FBlendViewFolderNode>);

	TArray<TWeakObjectPtr<AActor>> GetSelectedLevelActors()
	{
		TArray<TWeakObjectPtr<AActor>> Result;
		if (!GEditor)
		{
			return Result;
		}

		if (USelection* SelectedActors = GEditor->GetSelectedActors())
		{
			TArray<AActor*> Actors;
			SelectedActors->GetSelectedObjects<AActor>(Actors);
			for (AActor* Actor : Actors)
			{
				if (IsValid(Actor) && !Actor->IsTemplate())
				{
					Result.Add(Actor);
				}
			}
		}
		return Result;
	}

	UWorld* ResolveWorld(const TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : Actors)
		{
			if (AActor* Actor = WeakActor.Get())
			{
				return Actor->GetWorld();
			}
		}
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	FFolder::FRootObject ResolveRootObject(const TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : Actors)
		{
			if (const AActor* Actor = WeakActor.Get())
			{
				return Actor->GetFolderRootObject();
			}
		}
		return FFolder::GetInvalidRootObject();
	}

	TOptional<FName> ResolveSharedCurrentFolderPath(const TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		TOptional<FName> SharedPath;
		for (const TWeakObjectPtr<AActor>& WeakActor : Actors)
		{
			const AActor* Actor = WeakActor.Get();
			if (!IsValid(Actor) || Actor->IsTemplate())
			{
				continue;
			}

			const FName ActorPath = Actor->GetFolderPath();
			if (!SharedPath.IsSet())
			{
				SharedPath = ActorPath;
			}
			else if (SharedPath.GetValue() != ActorPath)
			{
				return TOptional<FName>();
			}
		}
		return SharedPath;
	}

	FName ParentPathOf(const FName Path)
	{
		FString PathString = Path.ToString();
		int32 SlashIndex = INDEX_NONE;
		return PathString.FindLastChar(TEXT('/'), SlashIndex)
			? FName(*PathString.Left(SlashIndex))
			: NAME_None;
	}

	FText LeafDisplayName(const FName Path)
	{
		FString PathString = Path.ToString();
		int32 SlashIndex = INDEX_NONE;
		if (PathString.FindLastChar(TEXT('/'), SlashIndex))
		{
			PathString.RightChopInline(SlashIndex + 1);
		}
		return FText::FromString(PathString);
	}

	FText FullFolderDisplayName(const FName Path)
	{
		FString PathString = Path.ToString();
		return FText::FromString(PathString.Replace(TEXT("/"), TEXT(" / ")));
	}

	FText RootDisplayName(const UWorld* World)
	{
		return World ? FText::FromString(World->GetName()) : FBlendViewLocalization::Text(TEXT("关卡"), TEXT("Level"));
	}

	void SortNodes(TArray<TSharedPtr<FBlendViewFolderNode>>& Nodes)
	{
		Nodes.Sort([](
			const TSharedPtr<FBlendViewFolderNode>& A,
			const TSharedPtr<FBlendViewFolderNode>& B)
		{
			if (!A.IsValid() || !B.IsValid())
			{
				return A.IsValid();
			}
			return A->SearchText < B->SearchText;
		});

		for (const TSharedPtr<FBlendViewFolderNode>& Node : Nodes)
		{
			if (Node.IsValid())
			{
				SortNodes(Node->Children);
			}
		}
	}

	void RefreshEditorAfterFolderMove()
	{
		if (GEditor)
		{
			GEditor->NoteSelectionChange();
			GEditor->RedrawLevelEditingViewports(false);
		}
		FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();
		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	void MoveActorsToFolder(
		const TArray<TWeakObjectPtr<AActor>>& Actors,
		const FFolder& Folder)
	{
		if (!Folder.IsValid())
		{
			return;
		}

		const FScopedTransaction Transaction(
			FBlendViewLocalization::Text(TEXT("移动到文件夹"), TEXT("Move to Folder")));
		USelection* SelectedActors = GEditor ? GEditor->GetSelectedActors() : nullptr;
		const FFolder::FRootObject RootObject = Folder.GetRootObject();
		const FName Path = Folder.GetPath();

		for (const TWeakObjectPtr<AActor>& WeakActor : Actors)
		{
			AActor* Actor = WeakActor.Get();
			if (!IsValid(Actor) || Actor->IsTemplate() || Actor->GetFolderRootObject() != RootObject)
			{
				continue;
			}

			const AActor* ParentActor = Actor->GetAttachParentActor();
			if (ParentActor && SelectedActors && SelectedActors->IsSelected(ParentActor))
			{
				continue;
			}

			Actor->Modify();
			Actor->SetFolderPath_Recursively(Path);
		}

		RefreshEditorAfterFolderMove();
	}

	void CreateFolderAndMoveActors(
		UWorld& World,
		const FFolder::FRootObject& RootObject,
		const TArray<TWeakObjectPtr<AActor>>& Actors,
		const FName ParentPath,
		const FString& RawFolderName)
	{
		const FString FolderName = RawFolderName.TrimStartAndEnd();
		if (FolderName.IsEmpty())
		{
			return;
		}

		const FFolder NewFolder = FActorFolders::Get().GetFolderName(
			World,
			FFolder(RootObject, ParentPath),
			FName(*FolderName));
		FActorFolders::Get().CreateFolder(World, NewFolder);
		MoveActorsToFolder(Actors, NewFolder);
	}

	class SBlendViewNewFolderMenu final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlendViewNewFolderMenu) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
			SLATE_ARGUMENT(TArray<TWeakObjectPtr<AActor>>, Actors)
			SLATE_ARGUMENT(FFolder::FRootObject, RootObject)
			SLATE_ARGUMENT(FName, ParentPath)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			World = InArgs._World;
			Actors = InArgs._Actors;
			RootObject = InArgs._RootObject;
			ParentPath = InArgs._ParentPath;

			ChildSlot
			[
				MakeMenuShell(
					SNew(SBox)
					.WidthOverride(260.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(5.0f, 3.0f, 5.0f, 8.0f))
						[
							SNew(STextBlock)
							.Text(FBlendViewLocalization::Text(TEXT("移动到新文件夹"), TEXT("Move to New Folder")))
							.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
						[
							MakeSeparator()
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 3.0f, 0.0f, 12.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(FBlendViewLocalization::Text(TEXT("名称"), TEXT("Name")))
								.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SAssignNew(NameTextBox, SEditableTextBox)
								.Text(FText::FromString(TEXT("New Folder")))
								.SelectAllTextWhenFocused(true)
								.OnTextCommitted(this, &SBlendViewNewFolderMenu::OnNameCommitted)
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 2.0f, 0.0f, 1.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), TEXT("PrimaryButton"))
								.HAlign(HAlign_Center)
								.Text(FBlendViewLocalization::Text(TEXT("创建"), TEXT("Create")))
								.OnClicked(this, &SBlendViewNewFolderMenu::OnCreateClicked)
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), TEXT("Button"))
								.HAlign(HAlign_Center)
								.Text(FBlendViewLocalization::Text(TEXT("取消"), TEXT("Cancel")))
								.OnClicked(this, &SBlendViewNewFolderMenu::OnCancelClicked)
							]
						]
					])
			];

			RegisterActiveTimer(
				0.0f,
				FWidgetActiveTimerDelegate::CreateSP(this, &SBlendViewNewFolderMenu::FocusNameTextBox));
		}

	private:
		EActiveTimerReturnType FocusNameTextBox(double, float)
		{
			if (NameTextBox.IsValid() && FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().SetKeyboardFocus(NameTextBox, EFocusCause::SetDirectly);
				NameTextBox->SelectAllText();
			}
			return EActiveTimerReturnType::Stop;
		}

		void OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType)
		{
			if (CommitType == ETextCommit::OnEnter)
			{
				CreateFromName(NewText);
			}
		}

		FReply OnCreateClicked()
		{
			CreateFromName(NameTextBox.IsValid() ? NameTextBox->GetText() : FText::GetEmpty());
			return FReply::Handled();
		}

		FReply OnCancelClicked()
		{
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().DismissAllMenus();
			}
			return FReply::Handled();
		}

		void CreateFromName(const FText& Name)
		{
			if (UWorld* PinnedWorld = World.Get())
			{
				CreateFolderAndMoveActors(*PinnedWorld, RootObject, Actors, ParentPath, Name.ToString());
			}
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().DismissAllMenus();
			}
		}

		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AActor>> Actors;
		FFolder::FRootObject RootObject;
		FName ParentPath;
		TSharedPtr<SEditableTextBox> NameTextBox;
	};

	class SBlendViewFolderMenuRow final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlendViewFolderMenuRow) {}
			SLATE_ARGUMENT(TSharedPtr<FBlendViewFolderNode>, Node)
			SLATE_ARGUMENT(bool, CanMoveToFolder)
			SLATE_EVENT(FBlendViewMoveToFolderDelegate, OnMoveToFolder)
			SLATE_EVENT(FBlendViewBuildSubMenuDelegate, OnBuildSubMenu)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Node = InArgs._Node;
			bCanMoveToFolder = InArgs._CanMoveToFolder;
			OnMoveToFolder = InArgs._OnMoveToFolder;
			OnBuildSubMenu = InArgs._OnBuildSubMenu;

			ChildSlot
			[
				SAssignNew(MenuAnchor, SMenuAnchor)
				.Placement(MenuPlacement_MenuRight)
				.Method(EPopupMethod::UseCurrentWindow)
				.ShowMenuBackground(false)
				.OnGetMenuContent(this, &SBlendViewFolderMenuRow::BuildSubMenu)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), TEXT("Menu.Button"))
					.ContentPadding(FMargin(7.0f, 4.0f))
					.OnClicked(this, &SBlendViewFolderMenuRow::OnClicked)
					.OnHovered(this, &SBlendViewFolderMenuRow::OnHovered)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.0f, 0.0f, 7.0f, 0.0f))
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush(FolderIconBrush))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Node.IsValid() ? Node->Label : FText::GetEmpty())
							.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush(SubMenuArrowBrush))
							.ColorAndOpacity(FSlateColor::UseForeground())
							.Visibility(this, &SBlendViewFolderMenuRow::GetArrowVisibility)
						]
					]
				]
			];
		}

	private:
		EVisibility GetArrowVisibility() const
		{
			return Node.IsValid() && !Node->Children.IsEmpty()
				? EVisibility::Visible
				: EVisibility::Hidden;
		}

		TSharedRef<SWidget> BuildSubMenu()
		{
			if (Node.IsValid() && !Node->Children.IsEmpty() && OnBuildSubMenu.IsBound())
			{
				return OnBuildSubMenu.Execute(Node);
			}
			return SNullWidget::NullWidget;
		}

		void OnHovered()
		{
			if (MenuAnchor.IsValid() && Node.IsValid() && !Node->Children.IsEmpty())
			{
				MenuAnchor->SetIsOpen(true, false);
			}
		}

		FReply OnClicked()
		{
			if (bCanMoveToFolder && Node.IsValid() && OnMoveToFolder.IsBound())
			{
				OnMoveToFolder.Execute(Node->Path);
				if (FSlateApplication::IsInitialized())
				{
					FSlateApplication::Get().DismissAllMenus();
				}
			}
			return FReply::Handled();
		}

		TSharedPtr<FBlendViewFolderNode> Node;
		bool bCanMoveToFolder = true;
		FBlendViewMoveToFolderDelegate OnMoveToFolder;
		FBlendViewBuildSubMenuDelegate OnBuildSubMenu;
		TSharedPtr<SMenuAnchor> MenuAnchor;
	};

	class SBlendViewFolderMenuPanel final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlendViewFolderMenuPanel) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
			SLATE_ARGUMENT(TArray<TWeakObjectPtr<AActor>>, Actors)
			SLATE_ARGUMENT(FFolder::FRootObject, RootObject)
			SLATE_ARGUMENT(TOptional<FName>, ExcludedPath)
			SLATE_ARGUMENT(FName, ParentPath)
			SLATE_ARGUMENT(bool, IsRootMenu)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			World = InArgs._World;
			Actors = InArgs._Actors;
			RootObject = InArgs._RootObject;
			ExcludedPath = InArgs._ExcludedPath;
			ParentPath = InArgs._ParentPath;
			bIsRootMenu = InArgs._IsRootMenu;
			BuildFolderTree();

			TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
			if (bIsRootMenu)
			{
				Content->AddSlot()
				.AutoHeight()
				.Padding(FMargin(5.0f, 2.0f, 5.0f, 6.0f))
				[
					SNew(STextBlock)
					.Text(FBlendViewLocalization::Text(TEXT("移动到文件夹"), TEXT("Move to Folder")))
					.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
				];
				Content->AddSlot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
				[
					MakeSeparator()
				];
				Content->AddSlot()
				.AutoHeight()
				.Padding(FMargin(2.0f, 0.0f, 2.0f, 6.0f))
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(FBlendViewLocalization::Text(TEXT("搜索..."), TEXT("Search...")))
					.OnTextChanged(this, &SBlendViewFolderMenuPanel::OnSearchChanged)
				];
				Content->AddSlot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 5.0f))
				[
					MakeSeparator()
				];
			}

			Content->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 5.0f))
			[
				MakeNewFolderButton()
			];
			Content->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 5.0f))
			[
				MakeSeparator()
			];
			Content->AddSlot()
			.AutoHeight()
			.MaxHeight(360.0f)
			[
				SAssignNew(ItemBox, SScrollBox)
			];

			ChildSlot
			[
				MakeMenuShell(
					SNew(SBox)
					.WidthOverride(bIsRootMenu ? 266.0f : 250.0f)
					[
						Content
					])
			];

			RefreshItems();
			if (bIsRootMenu)
			{
				RegisterActiveTimer(
					0.0f,
					FWidgetActiveTimerDelegate::CreateSP(this, &SBlendViewFolderMenuPanel::FocusSearchBox));
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

		void BuildFolderTree()
		{
			RootNodes.Reset();
			FlatNodes.Reset();

			if (UWorld* PinnedWorld = World.Get())
			{
				TMap<FName, TSharedPtr<FBlendViewFolderNode>> NodesByPath;
				FActorFolders::Get().ForEachFolder(*PinnedWorld, [this, &NodesByPath](const FFolder& Folder)
				{
					if (Folder.GetRootObject() != RootObject || Folder.GetPath().IsNone())
					{
						return true;
					}

					const FName Path = Folder.GetPath();
					TSharedPtr<FBlendViewFolderNode>& Node = NodesByPath.FindOrAdd(Path);
					if (!Node.IsValid())
					{
						Node = MakeShared<FBlendViewFolderNode>();
					}
					Node->Path = Path;
					Node->Label = LeafDisplayName(Path);
					Node->SearchText = FullFolderDisplayName(Path).ToString();
					FlatNodes.Add(Node);
					return true;
				});

				for (const TPair<FName, TSharedPtr<FBlendViewFolderNode>>& Pair : NodesByPath)
				{
					const FName NodeParentPath = ParentPathOf(Pair.Key);
					if (TSharedPtr<FBlendViewFolderNode>* ParentNode = NodesByPath.Find(NodeParentPath))
					{
						if (ParentNode->IsValid())
						{
							(*ParentNode)->Children.Add(Pair.Value);
							continue;
						}
					}
					RootNodes.Add(Pair.Value);
				}
			}

			SortNodes(RootNodes);
		}

		TSharedRef<SWidget> MakeNewFolderButton()
		{
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("Menu.Button"))
				.ContentPadding(FMargin(7.0f, 4.0f))
				.OnClicked(this, &SBlendViewFolderMenuPanel::OnNewFolderClicked)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f, 0.0f, 7.0f, 0.0f))
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(NewFolderIconBrush))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FBlendViewLocalization::Text(TEXT("新建文件夹"), TEXT("New Folder")))
						.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
					]
				];
		}

		void OnSearchChanged(const FText& NewText)
		{
			SearchText = NewText.ToString();
			RefreshItems();
		}

		void RefreshItems()
		{
			if (!ItemBox.IsValid())
			{
				return;
			}

			ItemBox->ClearChildren();
			if (bIsRootMenu && SearchText.IsEmpty())
			{
				AddRootItem();
			}

			const TArray<TSharedPtr<FBlendViewFolderNode>>& Nodes =
				bIsRootMenu
					? (SearchText.IsEmpty() ? RootNodes : FlatNodes)
					: FindChildrenForParent();

			for (const TSharedPtr<FBlendViewFolderNode>& Node : Nodes)
			{
				if (!Node.IsValid())
				{
					continue;
				}
				if (bIsRootMenu && !SearchText.IsEmpty() &&
					!Node->SearchText.Contains(SearchText, ESearchCase::IgnoreCase))
				{
					continue;
				}

				const bool bIsCurrentFolder = ExcludedPath.IsSet() && ExcludedPath.GetValue() == Node->Path;
				if (bIsCurrentFolder && Node->Children.IsEmpty())
				{
					continue;
				}

				ItemBox->AddSlot()
				.Padding(FMargin(0.0f, 1.0f))
				[
					SNew(SBlendViewFolderMenuRow)
					.Node(Node)
					.CanMoveToFolder(!bIsCurrentFolder)
					.OnMoveToFolder(FBlendViewMoveToFolderDelegate::CreateSP(this, &SBlendViewFolderMenuPanel::MoveToFolderPath))
					.OnBuildSubMenu(FBlendViewBuildSubMenuDelegate::CreateSP(this, &SBlendViewFolderMenuPanel::BuildSubMenuForNode))
				];
			}
		}

		void AddRootItem()
		{
			if (ExcludedPath.IsSet() && ExcludedPath.GetValue().IsNone())
			{
				return;
			}

			ItemBox->AddSlot()
			.Padding(FMargin(0.0f, 1.0f))
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("Menu.Button"))
				.ContentPadding(FMargin(7.0f, 4.0f))
				.OnClicked(this, &SBlendViewFolderMenuPanel::OnRootClicked)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f, 0.0f, 7.0f, 0.0f))
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(LevelIconBrush))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(RootDisplayName(World.Get()))
						.TextStyle(FAppStyle::Get(), TEXT("NormalText"))
					]
				]
			];
		}

		const TArray<TSharedPtr<FBlendViewFolderNode>>& FindChildrenForParent() const
		{
			static const TArray<TSharedPtr<FBlendViewFolderNode>> Empty;
			for (const TSharedPtr<FBlendViewFolderNode>& Node : FlatNodes)
			{
				if (Node.IsValid() && Node->Path == ParentPath)
				{
					return Node->Children;
				}
			}
			return Empty;
		}

		TSharedRef<SWidget> BuildSubMenuForNode(TSharedPtr<FBlendViewFolderNode> Node)
		{
			return SNew(SBlendViewFolderMenuPanel)
				.World(World)
				.Actors(Actors)
				.RootObject(RootObject)
				.ExcludedPath(ExcludedPath)
				.ParentPath(Node.IsValid() ? Node->Path : NAME_None)
				.IsRootMenu(false);
		}

		FReply OnRootClicked()
		{
			MoveToFolderPath(NAME_None);
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().DismissAllMenus();
			}
			return FReply::Handled();
		}

		void MoveToFolderPath(FName Path)
		{
			MoveActorsToFolder(Actors, FFolder(RootObject, Path));
		}

		FReply OnNewFolderClicked()
		{
			if (FSlateApplication::IsInitialized() && World.IsValid())
			{
				const FVector2D PopupPosition = FSlateApplication::Get().GetCursorPos() - NewFolderMenuSize * 0.5f;
				TSharedPtr<SWidget> ParentWidget = FSlateApplication::Get().GetActiveTopLevelWindow();
				if (!ParentWidget.IsValid())
				{
					ParentWidget = AsShared();
				}
				FSlateApplication::Get().DismissAllMenus();
				FSlateApplication::Get().PushMenu(
					ParentWidget.ToSharedRef(),
					FWidgetPath(),
					SNew(SBlendViewNewFolderMenu)
						.World(World)
						.Actors(Actors)
						.RootObject(RootObject)
						.ParentPath(ParentPath),
					PopupPosition,
					FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu),
					true,
					FVector2D::ZeroVector,
					EPopupMethod::UseCurrentWindow);
			}
			return FReply::Handled();
		}

		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AActor>> Actors;
		FFolder::FRootObject RootObject;
		TOptional<FName> ExcludedPath;
		FName ParentPath = NAME_None;
		bool bIsRootMenu = true;
		FString SearchText;
		TArray<TSharedPtr<FBlendViewFolderNode>> RootNodes;
		TArray<TSharedPtr<FBlendViewFolderNode>> FlatNodes;
		TSharedPtr<SSearchBox> SearchBox;
		TSharedPtr<SScrollBox> ItemBox;
	};
}

bool FBlendViewMoveToFolderMenu::Open(
	const FBlendViewViewportContext& ViewportContext,
	const FVector2D& ScreenPosition)
{
	if (!FSlateApplication::IsInitialized() || !GEditor)
	{
		return false;
	}

	TArray<TWeakObjectPtr<AActor>> Actors = GetSelectedLevelActors();
	if (Actors.IsEmpty())
	{
		return false;
	}

	UWorld* World = ResolveWorld(Actors);
	if (!World)
	{
		return false;
	}

	const FFolder::FRootObject RootObject = ResolveRootObject(Actors);
	if (!FFolder::IsRootObjectValid(RootObject))
	{
		return false;
	}

	TSharedPtr<SWidget> ParentWidget = ViewportContext.ViewportWidget.Pin();
	if (!ParentWidget.IsValid())
	{
		ParentWidget = FSlateApplication::Get().GetActiveTopLevelWindow();
	}
	if (!ParentWidget.IsValid())
	{
		return false;
	}

	FSlateApplication::Get().PushMenu(
		ParentWidget.ToSharedRef(),
		FWidgetPath(),
		SNew(SBlendViewFolderMenuPanel)
			.World(World)
			.Actors(Actors)
			.RootObject(RootObject)
			.ExcludedPath(ResolveSharedCurrentFolderPath(Actors))
			.ParentPath(NAME_None)
			.IsRootMenu(true),
		ScreenPosition,
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu),
		true,
		FVector2D::ZeroVector,
		EPopupMethod::UseCurrentWindow);
	return true;
}
