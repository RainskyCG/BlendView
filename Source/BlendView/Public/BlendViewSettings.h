// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BlendViewSettings.generated.h"

UENUM()
enum class EBlendViewTranslationNumericUnit : uint8
{
	Meters UMETA(DisplayName="Meters", ToolTip="Matches Blender's numeric translation unit."),
	Centimeters UMETA(DisplayName="Centimeters", ToolTip="Matches Unreal Engine world units.")
};

UENUM()
enum class EBlendViewSnapSourceMode : uint8
{
	Closest UMETA(DisplayName="Closest"),
	Pivot UMETA(DisplayName="Pivot")
};

UENUM()
enum class EBlendViewDisplayLanguage : uint8
{
	Auto UMETA(DisplayName="Auto", ToolTip="Follow the current Unreal Editor language."),
	Chinese UMETA(DisplayName="中文"),
	English UMETA(DisplayName="English")
};

UENUM()
enum class EBlendViewTransformPivotMode : uint8
{
	BoundingBoxCenter UMETA(DisplayName="Bounding Box Center"),
	Cursor UMETA(DisplayName="3D Cursor"),
	IndividualOrigins UMETA(DisplayName="Individual Origins"),
	ActiveItem UMETA(DisplayName="Active Element"),
	WorldOrigin UMETA(DisplayName="World Origin")
};

UENUM()
enum class EBlendViewQuickFavoriteContext : uint8
{
	Global UMETA(DisplayName="Global"),
	LevelViewport UMETA(DisplayName="Level Viewport"),
	GraphEditor UMETA(DisplayName="Graph Editor"),
	ContentBrowser UMETA(DisplayName="Content Browser")
};

UENUM()
enum class EBlendViewQuickFavoriteSource : uint8
{
	NativeCommand UMETA(DisplayName="Native Command"),
	EditorMode UMETA(DisplayName="Editor Mode"),
	ModeTool UMETA(DisplayName="Mode Tool"),
	BlendViewAction UMETA(DisplayName="BlendView Action")
};

USTRUCT()
struct FBlendViewQuickFavoriteCommand
{
	GENERATED_BODY()

	UPROPERTY()
	EBlendViewQuickFavoriteSource SourceType = EBlendViewQuickFavoriteSource::NativeCommand;

	UPROPERTY()
	FName StableId = NAME_None;

	UPROPERTY()
	FName BindingContext = NAME_None;

	UPROPERTY()
	FName CommandName = NAME_None;

	UPROPERTY()
	TArray<EBlendViewQuickFavoriteContext> AllowedContexts;
};

UCLASS(config=EditorSettings, meta=(DisplayName="BlendView"))
class BLENDVIEW_API UBlendViewSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBlendViewSettings();

	virtual void PostInitProperties() override;
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;
	double GetTranslationNumericUnitScale() const;

	UPROPERTY(EditAnywhere, config, Category="功能", meta=(
		DisplayName="启用变换工作流",
		ToolTip="启用 G/R/S、约束轴、数字输入等 Blender 风格变换操作。"))
	bool bEnableTransformWorkflow = true;

	UPROPERTY(EditAnywhere, config, Category="功能", meta=(
		DisplayName="启用鼠标视图导航",
		ToolTip="启用 MMB 轨道、Shift+MMB 平移、Ctrl+MMB 推拉视图导航。"))
	bool bEnableMouseNavigation = true;

	UPROPERTY(EditAnywhere, config, Category="功能", meta=(
		DisplayName="显示居中工具栏",
		ToolTip="在关卡视口顶部居中显示 BlendView 的轴心点和吸附快捷工具栏。"))
	bool bShowCenterToolbar = true;

	UPROPERTY(EditAnywhere, config, Category="视图导航", meta=(
		DisplayName="轨道旋转灵敏度",
		ToolTip="BlendView 中键轨道旋转的每像素角度。所有支持的编辑器视口共用此值。",
		ClampMin="0.01", ClampMax="1.0", UIMin="0.05", UIMax="0.5"))
	float OrbitSensitivity = 0.25f;

	UPROPERTY(EditAnywhere, config, Category="视图导航", meta=(
		DisplayName="反转轨道旋转 Y 轴",
		ToolTip="反转 BlendView 中键轨道旋转的垂直方向。"))
	bool bInvertOrbitYAxis = false;

	UPROPERTY(EditAnywhere, config, Category="视图导航", meta=(
		DisplayName="Shift/Alt 调整右键漫游速度",
		ToolTip="启用后，在右键 WASDQE 视口漫游时按住 Shift 会临时提高当前视口相机速度，按住 Alt 会临时降低当前视口相机速度；松开后恢复。"))
	bool bEnableShiftFlySpeedBoost = true;

	UPROPERTY(EditAnywhere, config, Category="视图导航", meta=(
		DisplayName="漫游速度系数",
		ToolTip="按住右键进行 WASDQE 漫游时，Shift 使用当前速度乘以该系数，Alt 使用当前速度除以该系数。",
		ClampMin="1.0", ClampMax="20.0", UIMin="1.0", UIMax="10.0"))
	float ShiftFlySpeedMultiplier = 5.0f;

	UPROPERTY(EditAnywhere, config, Category="界面", meta=(
		DisplayName="语言",
		ToolTip="设置 BlendView 设置页和菜单使用的语言。自动模式跟随 Unreal Editor 的当前语言。"))
	EBlendViewDisplayLanguage DisplayLanguage = EBlendViewDisplayLanguage::Auto;

	UPROPERTY(EditAnywhere, config, Category="实验功能", meta=(
		DisplayName="启用 3D 游标",
		ToolTip="启用 Shift+右键放置 BlendView 3D 游标，并允许游标绘制和游标相关命令。"))
	bool bEnableSceneCursor = true;

	UPROPERTY(EditAnywhere, config, Category="实验功能", meta=(
		DisplayName="启用游标与中心饼菜单",
		ToolTip="按 Shift+S 在关卡视口中弹出游标与中心饼菜单。"))
	bool bEnablePieMenu = true;

	UPROPERTY(EditAnywhere, config, Category="实验功能", meta=(
		DisplayName="启用快速收藏夹",
		ToolTip="按 Q 弹出 BlendView 快速收藏夹，并根据当前编辑器上下文显示可用命令。"))
	bool bEnableQuickFavorites = true;

	UPROPERTY(EditAnywhere, config, Category="实验功能", meta=(
		DisplayName="启用命令搜索",
		ToolTip="按 F3 弹出当前上下文的命令搜索菜单。"))
	bool bEnableCommandSearch = true;

	UPROPERTY(EditAnywhere, config, Category="实验功能", meta=(
		DisplayName="启用移动到文件夹菜单",
		ToolTip="按 M 在关卡视口中弹出移动到文件夹菜单。"))
	bool bEnableMoveToFolderMenu = true;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(
		DisplayName="启用 UE 吸附捕捉",
		ToolTip="开启后，BlendView 的 G/R/S 变换会读取当前 UE 网格、旋转和缩放吸附设置进行步进；关闭后不受 UE 全局吸附影响。"))
	bool bEnableUnrealEditorSnapCapture = false;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(
		DisplayName="吸附目标边缘限制",
		ToolTip="开启后，实际吸附目标只保留结构顶点和结构边，排除三角化对角线、平直结构边上的细分点和边中点；面命中保持不变。"))
	bool bEnableStructuralEdgeLimit = true;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(
		DisplayName="旋转对齐目标",
		ToolTip="开启后，移动临时吸附到几何目标时，会将对象本地 Z 轴对齐到吸附目标法线。"))
	bool bAlignRotationToSnapTarget = false;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(
		DisplayName="吸附轴粗细",
		ToolTip="吸附与约束轴线在 1920×1080、UI 缩放 1.0 时的基准粗细。实际显示仍按当前 UI/DPI 缩放自适应。",
		ClampMin="0.5", ClampMax="6.0", UIMin="0.5", UIMax="4.0"))
	float SnapAxisThickness = 3.0f;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(
		DisplayName="吸附基准",
		ToolTip="未手动设置吸附基准时，临时吸附使用的源点。枢轴点对应 Blender 的中心；最近会在找到吸附目标后使用离目标最近的选中项顶点。"))
	EBlendViewSnapSourceMode SnapSourceMode = EBlendViewSnapSourceMode::Pivot;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(DisplayName="吸附到网格"))
	bool bSnapTargetGrid = false;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(DisplayName="吸附到顶点"))
	bool bSnapTargetVertex = true;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(DisplayName="吸附到边"))
	bool bSnapTargetEdge = true;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(DisplayName="吸附到边中点"))
	bool bSnapTargetEdgeMidpoint = true;

	UPROPERTY(EditAnywhere, config, Category="吸附", meta=(DisplayName="吸附到面"))
	bool bSnapTargetFace = true;

	UPROPERTY(EditAnywhere, config, Category="变换", meta=(
		DisplayName="移动数值单位",
		ToolTip="G 移动时数值输入使用的单位。米与 Blender 一致；厘米与 Unreal Engine 世界单位一致。"))
	EBlendViewTranslationNumericUnit TranslationNumericUnit = EBlendViewTranslationNumericUnit::Meters;

	UPROPERTY(EditAnywhere, config, Category="变换", meta=(
		DisplayName="变换轴心点",
		ToolTip="旋转、缩放和镜像时使用联合边界框中心、3D 游标、各自原点、活动元素或世界原点。"))
	EBlendViewTransformPivotMode TransformPivotMode = EBlendViewTransformPivotMode::BoundingBoxCenter;

	UPROPERTY(config)
	int32 SettingsSchemaVersion = 0;

	UPROPERTY(config)
	TArray<FBlendViewQuickFavoriteCommand> QuickFavorites;
};
