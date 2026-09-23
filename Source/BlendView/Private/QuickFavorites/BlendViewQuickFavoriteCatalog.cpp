// Copyright 2026 RainskyCG. All Rights Reserved.

#include "QuickFavorites/BlendViewQuickFavoriteCatalog.h"

#include "BlendViewSettings.h"
#include "EditorModeRegistry.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Localization/BlendViewLocalization.h"
#include "Modules/ModuleManager.h"
#include "Snap/BlendViewSnapSolver.h"

namespace BlendViewQuickFavoriteCatalog
{
	namespace
	{
		FName MakeNativeStableId(const FName BindingContext, const FName CommandName)
		{
			return FName(*FString::Printf(TEXT("Native.%s.%s"), *BindingContext.ToString(), *CommandName.ToString()));
		}

		TArray<FEditorModeFavorite> GetEditorModeFavorites()
		{
			return {
				{
					TEXT("EditorMode.Selection"),
					FEditorModeID(TEXT("EM_Default")),
					NAME_None,
					FBlendViewLocalization::Text(TEXT("选择模式"), TEXT("Selection Mode")),
					FBlendViewLocalization::Text(TEXT("切换到关卡编辑器选择模式。"), TEXT("Switch to the level editor selection mode.")),
					FText::FromString(TEXT("Shift+1"))
				},
				{
					TEXT("EditorMode.Place"),
					FEditorModeID(TEXT("PLACEMENT")),
					TEXT("PlacementMode"),
					FBlendViewLocalization::Text(TEXT("放置模式"), TEXT("Place Mode")),
					FBlendViewLocalization::Text(TEXT("切换到放置 Actor 模式。"), TEXT("Switch to Place Actors mode.")),
					FText::FromString(TEXT("Shift+2"))
				},
				{
					TEXT("EditorMode.Landscape"),
					FEditorModeID(TEXT("EM_Landscape")),
					TEXT("LandscapeEditor"),
					FBlendViewLocalization::Text(TEXT("地形模式"), TEXT("Landscape Mode")),
					FBlendViewLocalization::Text(TEXT("切换到地形编辑模式。"), TEXT("Switch to Landscape mode.")),
					FText::FromString(TEXT("Shift+3"))
				},
				{
					TEXT("EditorMode.Foliage"),
					FEditorModeID(TEXT("EM_Foliage")),
					TEXT("FoliageEdit"),
					FBlendViewLocalization::Text(TEXT("植被模式"), TEXT("Foliage Mode")),
					FBlendViewLocalization::Text(TEXT("切换到植被编辑模式。"), TEXT("Switch to Foliage mode.")),
					FText::FromString(TEXT("Shift+4"))
				},
				{
					TEXT("EditorMode.Modeling"),
					FEditorModeID(TEXT("EM_ModelingToolsEditorMode")),
					TEXT("ModelingToolsEditorMode"),
					FBlendViewLocalization::Text(TEXT("建模模式"), TEXT("Modeling Mode")),
					FBlendViewLocalization::Text(TEXT("切换到建模工具模式。"), TEXT("Switch to Modeling Tools mode.")),
					FText::FromString(TEXT("Shift+5"))
				},
				{
					TEXT("EditorMode.MeshPaint"),
					FEditorModeID(TEXT("EM_MeshPaint")),
					TEXT("MeshPaint"),
					FBlendViewLocalization::Text(TEXT("网格绘制模式"), TEXT("Mesh Paint Mode")),
					FBlendViewLocalization::Text(TEXT("切换到网格绘制模式。"), TEXT("Switch to Mesh Paint mode.")),
					FText::FromString(TEXT("Shift+6"))
				}
			};
		}

		bool IsEditorModeRegistered(const FEditorModeID& ModeId)
		{
			return FEditorModeRegistry::Get().GetFactoryMap().Contains(ModeId);
		}

		bool IsRequiredEditorModeModuleAvailable(const FEditorModeFavorite& Favorite)
		{
			return IsModuleAvailable(Favorite.RequiredModuleName);
		}

		TArray<FModeToolFavorite> GetModeToolFavorites()
		{
			const FName ModelingModeStableId(TEXT("EditorMode.Modeling"));
			const FEditorModeID ModelingModeId(TEXT("EM_ModelingToolsEditorMode"));
			const FName ModelingToolsModule(TEXT("ModelingToolsEditorMode"));
			const FName ModelingToolsContext(TEXT("ModelingToolsManagerCommands"));

			return {
				{
					TEXT("ModeTool.Modeling.Transform"),
					ModelingModeStableId,
					ModelingModeId,
					ModelingToolsModule,
					ModelingToolsContext,
					TEXT("BeginTransformMeshesTool"),
					FText::FromString(TEXT("Transform")),
					FText::FromString(TEXT("Transform the selected meshes with the Modeling Tools transform tool.")),
					FText::GetEmpty()
				},
				{
					TEXT("ModeTool.Modeling.EditPivot"),
					ModelingModeStableId,
					ModelingModeId,
					ModelingToolsModule,
					ModelingToolsContext,
					TEXT("BeginEditPivotTool"),
					FText::FromString(TEXT("Edit Pivot")),
					FText::FromString(TEXT("Edit the pivot points of the selected meshes.")),
					FText::GetEmpty()
				}
			};
		}

		TArray<FActionFavorite> GetBlendViewActionFavorites()
		{
			const FText BlendViewContext = FText::FromString(TEXT("BlendView"));
			return {
				{
					TEXT("BlendView.Pivot.BoundingBoxCenter"),
					FBlendViewLocalization::Text(TEXT("轴心点：边界框中心"), TEXT("Pivot: Bounding Box Center")),
					FBlendViewLocalization::Text(TEXT("将 BlendView 变换轴心点设置为所选内容的边界框中心。"), TEXT("Set the BlendView transform pivot to the selected bounding box center.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.Pivot.Cursor"),
					FBlendViewLocalization::Text(TEXT("轴心点：3D 游标"), TEXT("Pivot: 3D Cursor")),
					FBlendViewLocalization::Text(TEXT("将 BlendView 变换轴心点设置为 3D 游标。"), TEXT("Set the BlendView transform pivot to the 3D cursor.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.Pivot.IndividualOrigins"),
					FBlendViewLocalization::Text(TEXT("轴心点：各自原点"), TEXT("Pivot: Individual Origins")),
					FBlendViewLocalization::Text(TEXT("将 BlendView 变换轴心点设置为各自原点。"), TEXT("Set the BlendView transform pivot to individual origins.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.Pivot.ActiveElement"),
					FBlendViewLocalization::Text(TEXT("轴心点：活动元素"), TEXT("Pivot: Active Element")),
					FBlendViewLocalization::Text(TEXT("将 BlendView 变换轴心点设置为活动元素。"), TEXT("Set the BlendView transform pivot to the active element.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.Pivot.WorldOrigin"),
					FBlendViewLocalization::Text(TEXT("轴心点：世界原点"), TEXT("Pivot: World Origin")),
					FBlendViewLocalization::Text(TEXT("将 BlendView 变换轴心点设置为世界原点。"), TEXT("Set the BlendView transform pivot to the world origin.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.SnapSource.Closest"),
					FBlendViewLocalization::Text(TEXT("吸附基准：最近"), TEXT("Snap Source: Closest")),
					FBlendViewLocalization::Text(TEXT("将 BlendView 吸附基准设置为最近点。"), TEXT("Set the BlendView snap source to closest point.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.SnapSource.Pivot"),
					FBlendViewLocalization::Text(TEXT("吸附基准：枢轴点"), TEXT("Snap Source: Pivot")),
					FBlendViewLocalization::Text(TEXT("将 BlendView 吸附基准设置为枢轴点。"), TEXT("Set the BlendView snap source to pivot.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.SnapTarget.Grid"),
					FBlendViewLocalization::Text(TEXT("切换吸附到网格"), TEXT("Toggle Snap to Grid")),
					FBlendViewLocalization::Text(TEXT("启用或禁用 BlendView 网格吸附目标。"), TEXT("Enable or disable the BlendView grid snap target.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.SnapTarget.Vertex"),
					FBlendViewLocalization::Text(TEXT("切换吸附到顶点"), TEXT("Toggle Snap to Vertex")),
					FBlendViewLocalization::Text(TEXT("启用或禁用 BlendView 顶点吸附目标。"), TEXT("Enable or disable the BlendView vertex snap target.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.SnapTarget.Edge"),
					FBlendViewLocalization::Text(TEXT("切换吸附到边"), TEXT("Toggle Snap to Edge")),
					FBlendViewLocalization::Text(TEXT("启用或禁用 BlendView 边吸附目标。"), TEXT("Enable or disable the BlendView edge snap target.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.SnapTarget.EdgeMidpoint"),
					FBlendViewLocalization::Text(TEXT("切换吸附到边中点"), TEXT("Toggle Snap to Edge Midpoint")),
					FBlendViewLocalization::Text(TEXT("启用或禁用 BlendView 边中点吸附目标。"), TEXT("Enable or disable the BlendView edge midpoint snap target.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.SnapTarget.Face"),
					FBlendViewLocalization::Text(TEXT("切换吸附到面"), TEXT("Toggle Snap to Face")),
					FBlendViewLocalization::Text(TEXT("启用或禁用 BlendView 面吸附目标。"), TEXT("Enable or disable the BlendView face snap target.")),
					BlendViewContext
				},
				{
					TEXT("BlendView.Snap.AlignRotationToTarget"),
					FBlendViewLocalization::Text(TEXT("切换旋转对齐目标"), TEXT("Toggle Align Rotation to Target")),
					FBlendViewLocalization::Text(TEXT("启用或禁用移动吸附时将旋转对齐到目标法线。"), TEXT("Enable or disable aligning rotation to the snap target normal.")),
					BlendViewContext
				}
			};
		}

		void SaveBlendViewSettings(UBlendViewSettings& Settings)
		{
			Settings.SaveConfig();
		}

		bool CanAssignShortcut(const FCommandItem& Item)
		{
			return (Item.SourceType == EBlendViewQuickFavoriteSource::NativeCommand ||
				Item.SourceType == EBlendViewQuickFavoriteSource::ModeTool) &&
				FindCommand(Item.BindingContext, Item.CommandName).IsValid();
		}
	}

	FText ContextDisplayName(const EBlendViewQuickFavoriteContext Context)
	{
		switch (Context)
		{
		case EBlendViewQuickFavoriteContext::LevelViewport:
			return FBlendViewLocalization::Text(TEXT("关卡视口"), TEXT("Level Viewport"));
		case EBlendViewQuickFavoriteContext::GraphEditor:
			return FBlendViewLocalization::Text(TEXT("图表编辑器"), TEXT("Graph Editor"));
		case EBlendViewQuickFavoriteContext::ContentBrowser:
			return FBlendViewLocalization::Text(TEXT("内容浏览器"), TEXT("Content Browser"));
		default:
			return FBlendViewLocalization::Text(TEXT("全局"), TEXT("Global"));
		}
	}

	bool AllowsContext(
		const FBlendViewQuickFavoriteCommand& Favorite,
		const EBlendViewQuickFavoriteContext Context)
	{
		return Favorite.AllowedContexts.Contains(EBlendViewQuickFavoriteContext::Global) ||
			Favorite.AllowedContexts.Contains(Context);
	}

	FName GetFavoriteStableId(const FBlendViewQuickFavoriteCommand& Favorite)
	{
		if (!Favorite.StableId.IsNone())
		{
			return Favorite.StableId;
		}
		if (Favorite.SourceType == EBlendViewQuickFavoriteSource::NativeCommand)
		{
			return MakeNativeStableId(Favorite.BindingContext, Favorite.CommandName);
		}
		return NAME_None;
	}

	bool IsModuleAvailable(const FName ModuleName)
	{
		if (ModuleName.IsNone())
		{
			return true;
		}

		return FModuleManager::Get().IsModuleLoaded(ModuleName) ||
			FModuleManager::Get().ModuleExists(*ModuleName.ToString());
	}

	bool IsEditorModeVisibleInFavorites(const FEditorModeFavorite& Favorite)
	{
		if (IsEditorModeRegistered(Favorite.ModeId))
		{
			return true;
		}

		// Editor modes are registered by their owning modules. The add-list should be a
		// stable catalog, not a snapshot of whichever modes have already registered.
		return IsRequiredEditorModeModuleAvailable(Favorite);
	}

	bool EnsureEditorModeRegistered(const FEditorModeFavorite& Favorite)
	{
		if (IsEditorModeRegistered(Favorite.ModeId))
		{
			return true;
		}

		if (!Favorite.RequiredModuleName.IsNone() && IsRequiredEditorModeModuleAvailable(Favorite))
		{
			FModuleManager::Get().LoadModule(Favorite.RequiredModuleName);
			return IsEditorModeRegistered(Favorite.ModeId);
		}

		if (Favorite.ModeId == FEditorModeID(TEXT("EM_Default")))
		{
			return true;
		}

		return false;
	}

	TOptional<FEditorModeFavorite> FindEditorModeFavorite(const FName StableId)
	{
		for (const FEditorModeFavorite& Favorite : GetEditorModeFavorites())
		{
			if (Favorite.StableId == StableId)
			{
				return Favorite;
			}
		}
		return TOptional<FEditorModeFavorite>();
	}

	TOptional<FModeToolFavorite> FindModeToolFavorite(const FName StableId)
	{
		for (const FModeToolFavorite& Favorite : GetModeToolFavorites())
		{
			if (Favorite.StableId == StableId)
			{
				return Favorite;
			}
		}
		return TOptional<FModeToolFavorite>();
	}

	TOptional<FActionFavorite> FindBlendViewActionFavorite(const FName StableId)
	{
		for (const FActionFavorite& Favorite : GetBlendViewActionFavorites())
		{
			if (Favorite.StableId == StableId)
			{
				return Favorite;
			}
		}
		return TOptional<FActionFavorite>();
	}

	bool ExecuteBlendViewAction(const FName StableId)
	{
		UBlendViewSettings* Settings = GetMutableDefault<UBlendViewSettings>();
		if (!Settings)
		{
			return false;
		}

		if (StableId == FName(TEXT("BlendView.Pivot.BoundingBoxCenter")))
		{
			Settings->TransformPivotMode = EBlendViewTransformPivotMode::BoundingBoxCenter;
		}
		else if (StableId == FName(TEXT("BlendView.Pivot.Cursor")))
		{
			Settings->TransformPivotMode = EBlendViewTransformPivotMode::Cursor;
		}
		else if (StableId == FName(TEXT("BlendView.Pivot.IndividualOrigins")))
		{
			Settings->TransformPivotMode = EBlendViewTransformPivotMode::IndividualOrigins;
		}
		else if (StableId == FName(TEXT("BlendView.Pivot.ActiveElement")))
		{
			Settings->TransformPivotMode = EBlendViewTransformPivotMode::ActiveItem;
		}
		else if (StableId == FName(TEXT("BlendView.Pivot.WorldOrigin")))
		{
			Settings->TransformPivotMode = EBlendViewTransformPivotMode::WorldOrigin;
		}
		else if (StableId == FName(TEXT("BlendView.SnapSource.Closest")))
		{
			Settings->SnapSourceMode = EBlendViewSnapSourceMode::Closest;
		}
		else if (StableId == FName(TEXT("BlendView.SnapSource.Pivot")))
		{
			Settings->SnapSourceMode = EBlendViewSnapSourceMode::Pivot;
		}
		else if (StableId == FName(TEXT("BlendView.SnapTarget.Grid")))
		{
			Settings->bSnapTargetGrid = !Settings->bSnapTargetGrid;
		}
		else if (StableId == FName(TEXT("BlendView.SnapTarget.Vertex")))
		{
			Settings->bSnapTargetVertex = !Settings->bSnapTargetVertex;
		}
		else if (StableId == FName(TEXT("BlendView.SnapTarget.Edge")))
		{
			Settings->bSnapTargetEdge = !Settings->bSnapTargetEdge;
		}
		else if (StableId == FName(TEXT("BlendView.SnapTarget.EdgeMidpoint")))
		{
			Settings->bSnapTargetEdgeMidpoint = !Settings->bSnapTargetEdgeMidpoint;
		}
		else if (StableId == FName(TEXT("BlendView.SnapTarget.Face")))
		{
			Settings->bSnapTargetFace = !Settings->bSnapTargetFace;
		}
		else if (StableId == FName(TEXT("BlendView.Snap.AlignRotationToTarget")))
		{
			Settings->bAlignRotationToSnapTarget = !Settings->bAlignRotationToSnapTarget;
		}
		else
		{
			return false;
		}

		SaveBlendViewSettings(*Settings);
		return true;
	}

	bool MatchesSearch(const FCommandItem& Item, const FString& SearchText)
	{
		if (SearchText.IsEmpty())
		{
			return true;
		}

		return Item.Label.ToString().Contains(SearchText, ESearchCase::IgnoreCase) ||
			Item.Description.ToString().Contains(SearchText, ESearchCase::IgnoreCase) ||
			Item.ContextLabel.ToString().Contains(SearchText, ESearchCase::IgnoreCase) ||
			Item.CommandName.ToString().Contains(SearchText, ESearchCase::IgnoreCase) ||
			Item.BindingContext.ToString().Contains(SearchText, ESearchCase::IgnoreCase);
	}

	TSharedPtr<FUICommandInfo> FindCommand(const FName BindingContext, const FName CommandName)
	{
		return FInputBindingManager::Get().FindCommandInContext(BindingContext, CommandName);
	}

	TSharedPtr<FUICommandInfo> FindShortcutCommand(const FCommandItem& Item)
	{
		return CanAssignShortcut(Item)
			? FindCommand(Item.BindingContext, Item.CommandName)
			: nullptr;
	}

	FCommandItem MakeNativeCommandItem(const TSharedRef<FUICommandInfo>& CommandInfo)
	{
		FCommandItem Item;
		Item.SourceType = EBlendViewQuickFavoriteSource::NativeCommand;
		Item.BindingContext = CommandInfo->GetBindingContext();
		Item.CommandName = CommandInfo->GetCommandName();
		Item.StableId = MakeNativeStableId(Item.BindingContext, Item.CommandName);
		Item.Label = CommandInfo->GetLabel();
		Item.Description = CommandInfo->GetDescription();
		if (const TSharedPtr<FBindingContext> BindingContext =
			FInputBindingManager::Get().GetContextByName(Item.BindingContext))
		{
			Item.ContextLabel = BindingContext->GetContextDesc();
		}
		else
		{
			Item.ContextLabel = FText::FromName(Item.BindingContext);
		}
		Item.InputText = CommandInfo->GetInputText();
		return Item;
	}

	FCommandItem MakeEditorModeItem(const FEditorModeFavorite& Favorite)
	{
		FCommandItem Item;
		Item.SourceType = EBlendViewQuickFavoriteSource::EditorMode;
		Item.StableId = Favorite.StableId;
		Item.Label = Favorite.Label;
		Item.Description = Favorite.Description;
		Item.ContextLabel = FBlendViewLocalization::Text(TEXT("编辑器模式"), TEXT("Editor Mode"));
		Item.InputText = Favorite.InputText;
		return Item;
	}

	FCommandItem MakeModeToolItem(const FModeToolFavorite& Favorite)
	{
		FCommandItem Item;
		Item.SourceType = EBlendViewQuickFavoriteSource::ModeTool;
		Item.StableId = Favorite.StableId;
		Item.BindingContext = Favorite.BindingContext;
		Item.CommandName = Favorite.CommandName;
		Item.Label = Favorite.Label;
		Item.Description = Favorite.Description;
		Item.ContextLabel = FBlendViewLocalization::Text(TEXT("模式工具"), TEXT("Mode Tool"));
		Item.InputText = Favorite.InputText;
		return Item;
	}

	FCommandItem MakeBlendViewActionItem(const FActionFavorite& Favorite)
	{
		FCommandItem Item;
		Item.SourceType = EBlendViewQuickFavoriteSource::BlendViewAction;
		Item.StableId = Favorite.StableId;
		Item.Label = Favorite.Label;
		Item.Description = Favorite.Description;
		Item.ContextLabel = Favorite.ContextLabel;
		return Item;
	}

	void GatherAllCommandItems(
		TArray<FCommandItem>& OutItems,
		const EBlendViewQuickFavoriteContext ActiveContext)
	{
		TArray<TSharedPtr<FBindingContext>> Contexts;
		FInputBindingManager::Get().GetKnownInputContexts(Contexts);
		for (const TSharedPtr<FBindingContext>& Context : Contexts)
		{
			if (!Context.IsValid())
			{
				continue;
			}

			TArray<TSharedPtr<FUICommandInfo>> Commands;
			FInputBindingManager::Get().GetCommandInfosFromContext(Context->GetContextName(), Commands);
			for (const TSharedPtr<FUICommandInfo>& CommandInfo : Commands)
			{
				if (CommandInfo.IsValid() &&
					FInputBindingManager::Get().CommandPassesFilter(
						CommandInfo->GetBindingContext(),
						CommandInfo->GetCommandName()))
				{
					OutItems.Add(MakeNativeCommandItem(CommandInfo.ToSharedRef()));
				}
			}
		}

		if (ActiveContext == EBlendViewQuickFavoriteContext::LevelViewport)
		{
			for (const FEditorModeFavorite& Favorite : GetEditorModeFavorites())
			{
				if (IsEditorModeVisibleInFavorites(Favorite))
				{
					OutItems.Add(MakeEditorModeItem(Favorite));
				}
			}

			for (const FModeToolFavorite& Favorite : GetModeToolFavorites())
			{
				if (IsModuleAvailable(Favorite.RequiredModuleName))
				{
					OutItems.Add(MakeModeToolItem(Favorite));
				}
			}

			for (const FActionFavorite& Favorite : GetBlendViewActionFavorites())
			{
				OutItems.Add(MakeBlendViewActionItem(Favorite));
			}
		}

		OutItems.Sort([](
			const FCommandItem& A,
			const FCommandItem& B)
		{
			const int32 ContextCompare = A.ContextLabel.ToString().Compare(B.ContextLabel.ToString());
			return ContextCompare == 0
				? A.Label.ToString() < B.Label.ToString()
				: ContextCompare < 0;
		});
	}

	bool IsDuplicateFavorite(
		const UBlendViewSettings& Settings,
		const EBlendViewQuickFavoriteSource SourceType,
		const FName StableId,
		const EBlendViewQuickFavoriteContext Context)
	{
		for (const FBlendViewQuickFavoriteCommand& Favorite : Settings.QuickFavorites)
		{
			if (Favorite.SourceType == SourceType &&
				GetFavoriteStableId(Favorite) == StableId &&
				AllowsContext(Favorite, Context))
			{
				return true;
			}
		}
		return false;
	}
}
