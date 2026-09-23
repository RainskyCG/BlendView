// Copyright 2026 RainskyCG. All Rights Reserved.

#include "BlendViewSettingsCustomization.h"

#include "BlendViewCommands.h"
#include "BlendViewSettings.h"
#include "Core/BlendViewFeatureSettings.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Graph/BlendViewGraphCommands.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "Localization/BlendViewLocalization.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "IDetailCustomization.h"
#include "InputCoreTypes.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BlendViewSettingsCustomization"

namespace BlendViewSettingsCustomizationPrivate
{
	bool bBlendViewSettingsCustomizationRegistered = false;

	void SetEnumEntryText(
		UEnum* Enum,
		const int64 Value,
		const FText& DisplayName,
		const FText& Tooltip = FText::GetEmpty())
	{
		if (!Enum)
		{
			return;
		}

		const int32 Index = Enum->GetIndexByValue(Value);
		if (Index == INDEX_NONE)
		{
			return;
		}

		Enum->SetMetaData(TEXT("DisplayName"), *DisplayName.ToString(), Index);
		if (!Tooltip.IsEmpty())
		{
			Enum->SetMetaData(TEXT("ToolTip"), *Tooltip.ToString(), Index);
		}
	}

	void LocalizeEnumEntries()
	{
		SetEnumEntryText(
			StaticEnum<EBlendViewDisplayLanguage>(),
			static_cast<int64>(EBlendViewDisplayLanguage::Auto),
			FBlendViewLocalization::Text(TEXT("自动"), TEXT("Auto")),
			FBlendViewLocalization::Text(
				TEXT("跟随 Unreal Editor 当前语言。"),
				TEXT("Follow the current Unreal Editor language.")));
		SetEnumEntryText(
			StaticEnum<EBlendViewDisplayLanguage>(),
			static_cast<int64>(EBlendViewDisplayLanguage::Chinese),
			FText::FromString(TEXT("中文")));
		SetEnumEntryText(
			StaticEnum<EBlendViewDisplayLanguage>(),
			static_cast<int64>(EBlendViewDisplayLanguage::English),
			FText::FromString(TEXT("English")));

		SetEnumEntryText(
			StaticEnum<EBlendViewSnapSourceMode>(),
			static_cast<int64>(EBlendViewSnapSourceMode::Closest),
			FBlendViewLocalization::Text(TEXT("最近"), TEXT("Closest")),
			FBlendViewLocalization::Text(
				TEXT("使用当前选中项中离吸附目标最近的顶点。"),
				TEXT("Use the selected vertex closest to the snap target.")));
		SetEnumEntryText(
			StaticEnum<EBlendViewSnapSourceMode>(),
			static_cast<int64>(EBlendViewSnapSourceMode::Pivot),
			FBlendViewLocalization::Text(TEXT("枢轴点"), TEXT("Pivot")),
			FBlendViewLocalization::Text(
				TEXT("使用当前变换枢轴点。"),
				TEXT("Use the current transform pivot.")));

		SetEnumEntryText(
			StaticEnum<EBlendViewTranslationNumericUnit>(),
			static_cast<int64>(EBlendViewTranslationNumericUnit::Meters),
			FBlendViewLocalization::Text(TEXT("米"), TEXT("Meters")),
			FBlendViewLocalization::Text(
				TEXT("与 Blender 的移动数值输入单位一致。"),
				TEXT("Matches Blender's numeric translation unit.")));
		SetEnumEntryText(
			StaticEnum<EBlendViewTranslationNumericUnit>(),
			static_cast<int64>(EBlendViewTranslationNumericUnit::Centimeters),
			FBlendViewLocalization::Text(TEXT("厘米"), TEXT("Centimeters")),
			FBlendViewLocalization::Text(
				TEXT("与 Unreal Engine 世界单位一致。"),
				TEXT("Matches Unreal Engine world units.")));

		SetEnumEntryText(
			StaticEnum<EBlendViewTransformPivotMode>(),
			static_cast<int64>(EBlendViewTransformPivotMode::BoundingBoxCenter),
			FBlendViewLocalization::Text(TEXT("边界框中心"), TEXT("Bounding Box Center")),
			FBlendViewLocalization::Text(
				TEXT("围绕所有选中对象联合边界框的中心整体旋转或缩放。"),
				TEXT("Rotate or scale the selection around its combined bounding-box center.")));
		SetEnumEntryText(
			StaticEnum<EBlendViewTransformPivotMode>(),
			static_cast<int64>(EBlendViewTransformPivotMode::Cursor),
			FBlendViewLocalization::Text(TEXT("3D 游标"), TEXT("3D Cursor")),
			FBlendViewLocalization::Text(
				TEXT("围绕 BlendView 3D 游标位置旋转、缩放或镜像选中对象。"),
				TEXT("Rotate, scale, or mirror the selection around the BlendView 3D cursor.")));
		SetEnumEntryText(
			StaticEnum<EBlendViewTransformPivotMode>(),
			static_cast<int64>(EBlendViewTransformPivotMode::IndividualOrigins),
			FBlendViewLocalization::Text(TEXT("各自的原点"), TEXT("Individual Origins")),
			FBlendViewLocalization::Text(
				TEXT("每个选中对象围绕自身原点独立旋转或缩放。"),
				TEXT("Rotate or scale each selected object independently around its own origin.")));
		SetEnumEntryText(
			StaticEnum<EBlendViewTransformPivotMode>(),
			static_cast<int64>(EBlendViewTransformPivotMode::ActiveItem),
			FBlendViewLocalization::Text(TEXT("活动元素"), TEXT("Active Element")),
			FBlendViewLocalization::Text(
				TEXT("围绕最后选中的活动元素原点旋转、缩放或镜像选中对象。"),
				TEXT("Rotate, scale, or mirror the selection around the last selected active element.")));
		SetEnumEntryText(
			StaticEnum<EBlendViewTransformPivotMode>(),
			static_cast<int64>(EBlendViewTransformPivotMode::WorldOrigin),
			FBlendViewLocalization::Text(TEXT("世界原点"), TEXT("World Origin")),
			FBlendViewLocalization::Text(
				TEXT("围绕世界坐标原点旋转、缩放或镜像选中对象。"),
				TEXT("Rotate, scale, or mirror the selection around the world origin.")));

	}

	void SortSettingsCategories(const TMap<FName, IDetailCategoryBuilder*>& Categories)
	{
		static const TArray<FName> CategoryOrder = {
			TEXT("界面"),
			TEXT("功能"),
			TEXT("快捷按键"),
			TEXT("视图导航"),
			TEXT("吸附"),
			TEXT("变换"),
			TEXT("实验功能"),
			TEXT("调试")
		};

		for (int32 Index = 0; Index < CategoryOrder.Num(); ++Index)
		{
			if (IDetailCategoryBuilder* const* Category = Categories.Find(CategoryOrder[Index]))
			{
				(*Category)->SetSortOrder(Index);
			}
		}
	}

	void LocalizeProperty(
		IDetailLayoutBuilder& DetailBuilder,
		const FName PropertyName,
		const TCHAR* ChineseName,
		const TCHAR* EnglishName,
		const TCHAR* ChineseTooltip,
		const TCHAR* EnglishTooltip)
	{
		const TSharedRef<IPropertyHandle> Property = DetailBuilder.GetProperty(PropertyName);
		if (!Property->IsValidHandle())
		{
			return;
		}

		Property->SetPropertyDisplayName(FBlendViewLocalization::Text(ChineseName, EnglishName));
		if (*ChineseTooltip && *EnglishTooltip)
		{
			Property->SetToolTipText(FBlendViewLocalization::Text(ChineseTooltip, EnglishTooltip));
		}
	}

	void LocalizeSettingsProperties(IDetailLayoutBuilder& DetailBuilder)
	{
		DetailBuilder.EditCategory(TEXT("功能"), FBlendViewLocalization::Text(TEXT("功能"), TEXT("Features")), ECategoryPriority::Important);
		DetailBuilder.EditCategory(TEXT("界面"), FBlendViewLocalization::Text(TEXT("界面"), TEXT("Interface")), ECategoryPriority::Important);
		DetailBuilder.EditCategory(TEXT("视图导航"), FBlendViewLocalization::Text(TEXT("视图导航"), TEXT("View Navigation")));
		DetailBuilder.EditCategory(TEXT("吸附"), FBlendViewLocalization::Text(TEXT("吸附"), TEXT("Snapping")));
		DetailBuilder.EditCategory(TEXT("变换"), FBlendViewLocalization::Text(TEXT("变换"), TEXT("Transform")));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableTransformWorkflow),
			TEXT("启用变换工作流"), TEXT("Enable Transform Workflow"),
			TEXT("启用 G/R/S、约束轴、数字输入等 Blender 风格变换操作。"),
			TEXT("Enable Blender-style G/R/S transforms, axis constraints, and numeric input."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableMouseNavigation),
			TEXT("启用鼠标视图导航"), TEXT("Enable Mouse View Navigation"),
			TEXT("启用 MMB 轨道、Shift+MMB 平移、Ctrl+MMB 推拉视图导航。"),
			TEXT("Enable MMB orbit, Shift+MMB pan, and Ctrl+MMB dolly navigation."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bShowCenterToolbar),
			TEXT("显示居中工具栏"), TEXT("Show Center Toolbar"),
			TEXT("在关卡视口顶部居中显示 BlendView 的轴心点和吸附快捷工具栏。"),
			TEXT("Show BlendView pivot and snapping shortcut controls centered at the top of the level viewport."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, DisplayLanguage),
			TEXT("语言"), TEXT("Language"),
			TEXT("设置 BlendView 设置页和菜单使用的语言。自动模式跟随 Unreal Editor 的当前语言。"),
			TEXT("Set the language used by BlendView settings and menus. Auto follows the current Unreal Editor language."));
		for (const FBlendViewFeatureToggleDefinition& Feature : FBlendViewFeatureSettings::GetExperimentalFeatures())
		{
			LocalizeProperty(
				DetailBuilder,
				Feature.PropertyName,
				Feature.ChineseName,
				Feature.EnglishName,
				Feature.ChineseTooltip,
				Feature.EnglishTooltip);
		}
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, OrbitSensitivity),
			TEXT("轨道旋转灵敏度"), TEXT("Orbit Sensitivity"),
			TEXT("BlendView 中键轨道旋转的每像素角度。所有支持的编辑器视口共用此值。"),
			TEXT("Degrees per pixel for BlendView MMB orbit. Shared by all supported editor viewports."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bInvertOrbitYAxis),
			TEXT("反转轨道旋转 Y 轴"), TEXT("Invert Orbit Y Axis"),
			TEXT("反转 BlendView 中键轨道旋转的垂直方向。"),
			TEXT("Invert the vertical direction of BlendView MMB orbit."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableShiftFlySpeedBoost),
			TEXT("Shift/Alt 调整右键漫游速度"), TEXT("Shift/Alt Right-Mouse Fly Speed"),
			TEXT("启用后，在右键 WASDQE 视口漫游时按住 Shift 会临时提高当前视口相机速度，按住 Alt 会临时降低当前视口相机速度；松开后恢复。"),
			TEXT("Temporarily increase the active viewport camera speed with Shift or decrease it with Alt during right-mouse WASDQE fly navigation."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, ShiftFlySpeedMultiplier),
			TEXT("漫游速度系数"), TEXT("Fly Speed Factor"),
			TEXT("右键 WASDQE 漫游时，Shift 使用当前速度乘以该系数，Alt 使用当前速度除以该系数。"),
			TEXT("During right-mouse WASDQE fly navigation, Shift multiplies the current speed by this factor and Alt divides it by this factor."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableUnrealEditorSnapCapture),
			TEXT("启用 UE 吸附捕捉"), TEXT("Enable UE Snap Capture"),
			TEXT("开启后，BlendView 的 G/R/S 变换会读取当前 UE 网格、旋转和缩放吸附设置进行步进；关闭后不受 UE 全局吸附影响。"),
			TEXT("Use the current UE grid, rotation, and scale snap increments during BlendView G/R/S transforms."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bEnableStructuralEdgeLimit),
			TEXT("吸附目标边缘限制"), TEXT("Structural Edge Filter"),
			TEXT("开启后，实际吸附目标只保留结构顶点和结构边；面命中保持不变。"),
			TEXT("Limit transform snap targets to structural vertices and edges. Face hits are unchanged."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bAlignRotationToSnapTarget),
			TEXT("旋转对齐目标"), TEXT("Align Rotation to Target"),
			TEXT("开启后，移动临时吸附到几何目标时，会将对象本地 Z 轴对齐到吸附目标法线。"),
			TEXT("Align each object's local Z axis to the snap target normal while temporary translation snapping to geometry."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, SnapAxisThickness),
			TEXT("吸附轴粗细"), TEXT("Snap Axis Thickness"),
			TEXT("吸附与约束轴线在 1920×1080、UI 缩放 1.0 时的基准粗细。实际显示仍按当前 UI/DPI 缩放自适应。"),
			TEXT("Base snap and constraint axis thickness at 1920x1080 and UI scale 1.0. The displayed thickness still adapts to the current UI/DPI scale."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, SnapSourceMode),
			TEXT("吸附基准"), TEXT("Snap Source"),
			TEXT("未手动设置吸附基准时，选择临时吸附使用的源点。"),
			TEXT("Choose the source point used for temporary snapping when no snap base is set manually."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bSnapTargetGrid), TEXT("吸附到网格"), TEXT("Snap to Grid"), TEXT("允许吸附到网格。"), TEXT("Allow snapping to the grid."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bSnapTargetVertex), TEXT("吸附到顶点"), TEXT("Snap to Vertex"), TEXT("允许吸附到顶点。"), TEXT("Allow snapping to vertices."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bSnapTargetEdge), TEXT("吸附到边"), TEXT("Snap to Edge"), TEXT("允许吸附到边。"), TEXT("Allow snapping to edges."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bSnapTargetEdgeMidpoint), TEXT("吸附到边中点"), TEXT("Snap to Edge Midpoint"), TEXT("允许吸附到边中点。"), TEXT("Allow snapping to edge midpoints."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, bSnapTargetFace), TEXT("吸附到面"), TEXT("Snap to Face"), TEXT("允许吸附到面。"), TEXT("Allow snapping to faces."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, TranslationNumericUnit),
			TEXT("移动数值单位"), TEXT("Translation Numeric Unit"),
			TEXT("G 移动时数值输入使用的单位。米与 Blender 一致；厘米与 Unreal Engine 世界单位一致。"),
			TEXT("Unit used by numeric input during G translation: meters match Blender; centimeters match Unreal world units."));
		LocalizeProperty(DetailBuilder, GET_MEMBER_NAME_CHECKED(UBlendViewSettings, TransformPivotMode),
			TEXT("变换轴心点"), TEXT("Transform Pivot"),
			TEXT("设置旋转、缩放和镜像时使用联合边界框中心、3D 游标、各自原点、活动元素或世界原点。"),
			TEXT("Use the combined bounding-box center, 3D cursor, individual origins, active element, or world origin for rotation, scaling, and mirroring."));
	}

	FInputChord GetActiveChord(const TSharedPtr<FUICommandInfo>& Command, const EMultipleKeyBindingIndex ChordIndex)
	{
		return Command.IsValid()
			? *Command->GetActiveChord(ChordIndex)
			: FInputChord();
	}

	void RemoveConflictingChord(const TSharedPtr<FUICommandInfo>& Command, const FInputChord& NewChord)
	{
		if (!Command.IsValid() || !NewChord.IsValidChord())
		{
			return;
		}

		const bool bCheckDefaultChord = false;
		const TSharedPtr<FUICommandInfo> FoundCommand =
			FInputBindingManager::Get().GetCommandInfoFromInputChord(
				Command->GetBindingContext(),
				NewChord,
				bCheckDefaultChord);

		if (!FoundCommand.IsValid() || FoundCommand->GetCommandName() == Command->GetCommandName())
		{
			return;
		}

		for (uint8 Index = 0; Index < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++Index)
		{
			const EMultipleKeyBindingIndex RemovableIndex = static_cast<EMultipleKeyBindingIndex>(Index);
			if (*FoundCommand->GetActiveChord(RemovableIndex) == NewChord)
			{
				FoundCommand->RemoveActiveChord(RemovableIndex);
			}
		}
	}

	class SBlendViewChordInput final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlendViewChordInput)
			: _ChordIndex(EMultipleKeyBindingIndex::Primary)
			, _HintText(FBlendViewLocalization::Text(TEXT("输入一个新绑定"), TEXT("Enter a new binding")))
		{
		}
			SLATE_ARGUMENT(TSharedPtr<FUICommandInfo>, Command)
			SLATE_ARGUMENT(EMultipleKeyBindingIndex, ChordIndex)
			SLATE_ARGUMENT(FText, HintText)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Command = InArgs._Command;
			ChordIndex = InArgs._ChordIndex;
			HintText = InArgs._HintText;

			BorderImageNormal = FAppStyle::GetBrush(TEXT("EditableTextBox.Background.Normal"));
			BorderImageHovered = FAppStyle::GetBrush(TEXT("EditableTextBox.Background.Hovered"));
			BorderImageFocused = FAppStyle::GetBrush(TEXT("EditableTextBox.Background.Focused"));

			ChildSlot
			[
				SNew(SBox)
				.WidthOverride(220.0f)
				[
					SNew(SBorder)
					.VAlign(VAlign_Center)
					.Padding(FMargin(4.0f, 2.0f))
					.BorderImage(this, &SBlendViewChordInput::GetBorderImage)
					.ForegroundColor(FAppStyle::GetSlateColor(TEXT("InvertedForeground")))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &SBlendViewChordInput::GetChordText)
							.ColorAndOpacity(this, &SBlendViewChordInput::GetChordTextColor)
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.Visibility(this, &SBlendViewChordInput::GetRemoveButtonVisibility)
							.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
							.ContentPadding(0.0f)
							.IsFocusable(false)
							.ToolTipText(FBlendViewLocalization::Text(TEXT("移除此绑定"), TEXT("Remove this binding")))
							.OnClicked(this, &SBlendViewChordInput::OnRemoveBindingClicked)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush(TEXT("Symbols.X")))
								.ColorAndOpacity(FLinearColor(0.7f, 0.0f, 0.0f, 0.75f))
							]
						]
					]
				]
			];
		}

		virtual bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (!bIsEditing)
			{
				if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
				{
					StartEditing();
					return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
				}
				return FReply::Handled();
			}

			const FKey MouseButton = MouseEvent.GetEffectingButton();
			if (MouseButton.IsValid() && MouseButton != EKeys::LeftMouseButton)
			{
				CommitChord(FInputChord(
					MouseButton,
					MouseEvent.IsShiftDown(),
					MouseEvent.IsControlDown(),
					MouseEvent.IsAltDown(),
					MouseEvent.IsCommandDown()));
				StopEditing();
				FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
			}
			return FReply::Handled();
		}

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override
		{
			if (!bIsEditing)
			{
				return FReply::Unhandled();
			}

			const FKey Key = KeyEvent.GetKey();
			if (Key == EKeys::Escape)
			{
				StopEditing();
				return FReply::Handled();
			}

			EditingChord = FInputChord(
				Key.IsModifierKey() ? EKeys::Invalid : Key,
				KeyEvent.IsShiftDown(),
				KeyEvent.IsControlDown(),
				KeyEvent.IsAltDown(),
				KeyEvent.IsCommandDown());

			if (EditingChord.IsValidChord())
			{
				CommitChord(EditingChord);
				StopEditing();
				FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
			}

			return FReply::Handled();
		}

		virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override
		{
			SCompoundWidget::OnFocusLost(InFocusEvent);
			StopEditing();
		}

	private:
		const FSlateBrush* GetBorderImage() const
		{
			if (HasKeyboardFocus() || bIsEditing)
			{
				return BorderImageFocused;
			}
			return IsHovered() ? BorderImageHovered : BorderImageNormal;
		}

		FText GetChordText() const
		{
			if (bIsEditing)
			{
				return EditingChord.IsValidChord()
					? EditingChord.GetInputText()
					: FBlendViewLocalization::Text(TEXT("按下快捷键..."), TEXT("Press shortcut..."));
			}

			const FInputChord ActiveChord = GetActiveChord(Command, ChordIndex);
			return ActiveChord.IsValidChord()
				? ActiveChord.GetInputText()
				: HintText;
		}

		FSlateColor GetChordTextColor() const
		{
			const FInputChord ActiveChord = GetActiveChord(Command, ChordIndex);
			if (bIsEditing || ActiveChord.IsValidChord())
			{
				return FSlateColor::UseForeground();
			}
			return FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f, 1.0f));
		}

		EVisibility GetRemoveButtonVisibility() const
		{
			const FInputChord ActiveChord = GetActiveChord(Command, ChordIndex);
			return !bIsEditing && ActiveChord.IsValidChord()
				? EVisibility::Visible
				: EVisibility::Hidden;
		}

		FReply OnRemoveBindingClicked()
		{
			if (Command.IsValid() && !bIsEditing)
			{
				Command->RemoveActiveChord(ChordIndex);
				FInputBindingManager::Get().SaveInputBindings();
			}
			return FReply::Handled();
		}

		void StartEditing()
		{
			bIsEditing = true;
			EditingChord = FInputChord();
		}

		void StopEditing()
		{
			bIsEditing = false;
			EditingChord = FInputChord();
		}

		void CommitChord(const FInputChord& NewChord)
		{
			if (!Command.IsValid() || !NewChord.IsValidChord())
			{
				return;
			}

			RemoveConflictingChord(Command, NewChord);
			Command->SetActiveChord(NewChord, ChordIndex);
			FInputBindingManager::Get().SaveInputBindings();
		}

		TSharedPtr<FUICommandInfo> Command;
		EMultipleKeyBindingIndex ChordIndex = EMultipleKeyBindingIndex::Primary;
		FInputChord EditingChord;
		FText HintText;
		const FSlateBrush* BorderImageNormal = nullptr;
		const FSlateBrush* BorderImageHovered = nullptr;
		const FSlateBrush* BorderImageFocused = nullptr;
		bool bIsEditing = false;
	};

	void AddCommandShortcutRow(
		IDetailCategoryBuilder& Category,
		const FText& FilterText,
		const FText& Label,
		const FText& Tooltip,
		const TSharedPtr<FUICommandInfo>& Command)
	{
		Category.AddCustomRow(FilterText)
			.NameContent()
			[
				SNew(STextBlock)
				.Text(Label)
				.ToolTipText(Tooltip)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(460.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBlendViewChordInput)
					.Command(Command)
					.ChordIndex(EMultipleKeyBindingIndex::Primary)
					.HintText(FBlendViewLocalization::Text(TEXT("输入一个新绑定"), TEXT("Enter a new binding")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBlendViewChordInput)
					.Command(Command)
					.ChordIndex(EMultipleKeyBindingIndex::Secondary)
					.HintText(FBlendViewLocalization::Text(TEXT("输入一个新绑定"), TEXT("Enter a new binding")))
				]
			];
	}

	void AddCommandShortcutRow(
		IDetailGroup& Group,
		const FText& Label,
		const FText& Tooltip,
		const TSharedPtr<FUICommandInfo>& Command)
	{
		Group.AddWidgetRow()
			.NameContent()
			[
				SNew(STextBlock)
				.Text(Label)
				.ToolTipText(Tooltip)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(460.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBlendViewChordInput)
					.Command(Command)
					.ChordIndex(EMultipleKeyBindingIndex::Primary)
					.HintText(FBlendViewLocalization::Text(TEXT("输入一个新绑定"), TEXT("Enter a new binding")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBlendViewChordInput)
					.Command(Command)
					.ChordIndex(EMultipleKeyBindingIndex::Secondary)
					.HintText(FBlendViewLocalization::Text(TEXT("输入一个新绑定"), TEXT("Enter a new binding")))
				]
			];
	}

	class FBlendViewSettingsCustomization final : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FBlendViewSettingsCustomization>();
		}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			LocalizeEnumEntries();
			LocalizeSettingsProperties(DetailBuilder);
			DetailBuilder.SortCategories(SortSettingsCategories);
			const TSharedRef<IPropertyHandle> LanguageProperty = DetailBuilder.GetProperty(
				GET_MEMBER_NAME_CHECKED(UBlendViewSettings, DisplayLanguage));
			const TWeakPtr<IPropertyUtilities> PropertyUtilities = DetailBuilder.GetPropertyUtilities();
			LanguageProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([PropertyUtilities]()
			{
				GetMutableDefault<UBlendViewSettings>()->SaveConfig();
				if (const TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
				{
					Utilities->ForceRefresh();
				}
			}));

			TArray<TSharedRef<IPropertyHandle>> ExperimentalFeatureProperties;
			for (const FBlendViewFeatureToggleDefinition& Feature : FBlendViewFeatureSettings::GetExperimentalFeatures())
			{
				TSharedRef<IPropertyHandle> Property = DetailBuilder.GetProperty(Feature.PropertyName);
				DetailBuilder.HideProperty(Property);
				ExperimentalFeatureProperties.Add(Property);
			}
			IDetailCategoryBuilder& FeatureCategory = DetailBuilder.EditCategory(
				TEXT("功能"),
				FBlendViewLocalization::Text(TEXT("功能"), TEXT("Features")),
				ECategoryPriority::Important);
			IDetailGroup& ExperimentalFeatureGroup = FeatureCategory.AddGroup(
				TEXT("BlendViewExperimentalFeatures"),
				FBlendViewLocalization::Text(TEXT("实验功能"), TEXT("Experimental Features")),
				false,
				false);
			for (const TSharedRef<IPropertyHandle>& Property : ExperimentalFeatureProperties)
			{
				ExperimentalFeatureGroup.AddPropertyRow(Property);
			}

			IDetailCategoryBuilder& ShortcutCategory = DetailBuilder.EditCategory(
				TEXT("快捷按键"),
				FBlendViewLocalization::Text(TEXT("快捷按键"), TEXT("Shortcuts")),
				ECategoryPriority::Important);

			const FBlendViewCommands* Commands = FBlendViewCommands::IsRegistered()
				? &FBlendViewCommands::Get()
				: nullptr;
			const FBlendViewGraphCommands* GraphCommands = FBlendViewGraphCommands::IsRegistered()
				? &FBlendViewGraphCommands::Get()
				: nullptr;

			IDetailGroup& GeneralShortcutGroup = ShortcutCategory.AddGroup(
				TEXT("BlendViewGeneralShortcuts"),
				FBlendViewLocalization::Text(TEXT("通用"), TEXT("General")),
				false,
				false);
			IDetailGroup& TransformShortcutGroup = ShortcutCategory.AddGroup(
				TEXT("BlendViewTransformShortcuts"),
				FBlendViewLocalization::Text(TEXT("变换"), TEXT("Transform")),
				false,
				false);

			AddCommandShortcutRow(
				GeneralShortcutGroup,
				FBlendViewLocalization::Text(TEXT("切换 BlendView"), TEXT("Toggle BlendView")),
				FBlendViewLocalization::Text(TEXT("启用或禁用 BlendView。"), TEXT("Enable or disable BlendView.")),
				Commands ? Commands->ToggleBlendView : nullptr);
			AddCommandShortcutRow(
				TransformShortcutGroup,
				FBlendViewLocalization::Text(TEXT("移动"), TEXT("Translate")),
				FBlendViewLocalization::Text(TEXT("进入移动变换，默认 G。"), TEXT("Begin translation. Default: G.")),
				Commands ? Commands->BeginTranslate : nullptr);
			AddCommandShortcutRow(
				TransformShortcutGroup,
				FBlendViewLocalization::Text(TEXT("旋转"), TEXT("Rotate")),
				FBlendViewLocalization::Text(TEXT("进入旋转变换，默认 R。"), TEXT("Begin rotation. Default: R.")),
				Commands ? Commands->BeginRotate : nullptr);
			AddCommandShortcutRow(
				TransformShortcutGroup,
				FBlendViewLocalization::Text(TEXT("缩放"), TEXT("Scale")),
				FBlendViewLocalization::Text(TEXT("进入缩放变换，默认 S。"), TEXT("Begin scaling. Default: S.")),
				Commands ? Commands->BeginScale : nullptr);
			AddCommandShortcutRow(
				TransformShortcutGroup,
				FBlendViewLocalization::Text(TEXT("交互镜像"), TEXT("Interactive Mirror")),
				FBlendViewLocalization::Text(TEXT("进入交互镜像变换，默认 Ctrl+M。"), TEXT("Begin interactive mirror transform. Default: Ctrl+M.")),
				Commands ? Commands->BeginMirror : nullptr);
			AddCommandShortcutRow(
				TransformShortcutGroup,
				FBlendViewLocalization::Text(TEXT("移动/编辑枢轴点"), TEXT("Move/Edit Pivot")),
				FBlendViewLocalization::Text(TEXT("在移动变换中切换枢轴点编辑，默认 O。"), TEXT("Toggle pivot editing during translation. Default: O.")),
				Commands ? Commands->TogglePivotEditMode : nullptr);
			AddCommandShortcutRow(
				TransformShortcutGroup,
				FBlendViewLocalization::Text(TEXT("设置吸附基准"), TEXT("Set Snap Base")),
				FBlendViewLocalization::Text(TEXT("在 G/R/S 变换中进入吸附基准拾取状态，默认 B。"), TEXT("Pick a snap base during G/R/S transforms. Default: B.")),
				Commands ? Commands->SetSnapBase : nullptr);
			AddCommandShortcutRow(
				TransformShortcutGroup,
				FBlendViewLocalization::Text(TEXT("显示/隐藏变换提示"), TEXT("Show/Hide Transform Hints")),
				FBlendViewLocalization::Text(TEXT("显示或隐藏底部变换状态提示，默认 H；仅当前编辑器会话生效。"), TEXT("Show or hide bottom transform hints. Default: H. Applies to this editor session only.")),
				Commands ? Commands->ToggleTransformStatusBar : nullptr);
			AddCommandShortcutRow(
				GeneralShortcutGroup,
				FBlendViewLocalization::Text(TEXT("快速收藏夹"), TEXT("Quick Favorites")),
				FBlendViewLocalization::Text(TEXT("打开当前上下文的快速收藏夹，默认 Q。"), TEXT("Open quick favorites for the current context. Default: Q.")),
				Commands ? Commands->OpenQuickFavorites : nullptr);
			AddCommandShortcutRow(
				GeneralShortcutGroup,
				FBlendViewLocalization::Text(TEXT("命令搜索"), TEXT("Command Search")),
				FBlendViewLocalization::Text(TEXT("打开当前上下文的命令搜索菜单，默认 F3。"), TEXT("Open command search for the current context. Default: F3.")),
				Commands ? Commands->OpenCommandSearch : nullptr);
			AddCommandShortcutRow(
				GeneralShortcutGroup,
				FBlendViewLocalization::Text(TEXT("定位所选"), TEXT("Frame Selected")),
				FBlendViewLocalization::Text(TEXT("在鼠标所在的视口、大纲或内容浏览器中定位当前所选，默认小键盘句点。"), TEXT("Frame the selection under the cursor in a viewport, outliner, or content browser. Default: Numpad Period.")),
				Commands ? Commands->FrameSelected : nullptr);
			AddCommandShortcutRow(
				GeneralShortcutGroup,
				FBlendViewLocalization::Text(TEXT("移动到文件夹"), TEXT("Move to Folder")),
				FBlendViewLocalization::Text(TEXT("在关卡视口中打开文件夹移动菜单，默认 M。"), TEXT("Open the move-to-folder menu in the level viewport. Default: M.")),
				Commands ? Commands->MoveSelectedToFolder : nullptr);
			AddCommandShortcutRow(
				TransformShortcutGroup,
				FBlendViewLocalization::Text(TEXT("复制并移动"), TEXT("Duplicate and Translate")),
				FBlendViewLocalization::Text(TEXT("复制当前选中的 Actor 或蓝图组件，并立即进入移动状态，默认 Shift+D。"), TEXT("Duplicate selected actors or Blueprint components and immediately begin translation. Default: Shift+D.")),
				Commands ? Commands->DuplicateAndTranslate : nullptr);

			IDetailGroup& GraphTransformGroup = ShortcutCategory.AddGroup(
				TEXT("BlendViewGraphTransformShortcuts"),
				FBlendViewLocalization::Text(TEXT("图表节点变换"), TEXT("Graph Node Transform")),
				false,
				false);
			AddCommandShortcutRow(
				GraphTransformGroup,
				FBlendViewLocalization::Text(TEXT("移动节点"), TEXT("Translate Nodes")),
				FBlendViewLocalization::Text(TEXT("在图表中移动选中节点，默认 G。"), TEXT("Translate selected graph nodes. Default: G.")),
				GraphCommands ? GraphCommands->BeginTranslate : nullptr);
			AddCommandShortcutRow(
				GraphTransformGroup,
				FBlendViewLocalization::Text(TEXT("旋转节点布局"), TEXT("Rotate Node Layout")),
				FBlendViewLocalization::Text(TEXT("围绕选中节点中心旋转节点布局，默认 R。"), TEXT("Rotate selected nodes around their center. Default: R.")),
				GraphCommands ? GraphCommands->BeginRotate : nullptr);
			AddCommandShortcutRow(
				GraphTransformGroup,
				FBlendViewLocalization::Text(TEXT("缩放节点布局"), TEXT("Scale Node Layout")),
				FBlendViewLocalization::Text(TEXT("围绕选中节点中心缩放节点间距，默认 S。"), TEXT("Scale spacing around the selected node center. Default: S.")),
				GraphCommands ? GraphCommands->BeginScale : nullptr);
			AddCommandShortcutRow(
				GraphTransformGroup,
				FBlendViewLocalization::Text(TEXT("复制并移动"), TEXT("Duplicate and Translate")),
				FBlendViewLocalization::Text(TEXT("复制选中的图表节点并立即进入移动状态，默认 Shift+D。"), TEXT("Duplicate selected graph nodes and immediately begin translation. Default: Shift+D.")),
				GraphCommands ? GraphCommands->DuplicateAndTranslate : nullptr);
			AddCommandShortcutRow(
				GraphTransformGroup,
				FBlendViewLocalization::Text(TEXT("删除并重连节点"), TEXT("Delete and Reconnect Nodes")),
				FBlendViewLocalization::Text(TEXT("在蓝图图表中删除当前选中节点，并重连执行引脚，默认 Ctrl+X。"), TEXT("Delete selected Blueprint graph nodes and reconnect execution pins. Default: Ctrl+X.")),
				GraphCommands ? GraphCommands->DeleteAndReconnect : nullptr);
		}
	};
}

void RegisterBlendViewSettingsCustomization()
{
	if (BlendViewSettingsCustomizationPrivate::bBlendViewSettingsCustomizationRegistered)
	{
		return;
	}

	FPropertyEditorModule& PropertyModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomClassLayout(
		UBlendViewSettings::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(
			&BlendViewSettingsCustomizationPrivate::FBlendViewSettingsCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
	BlendViewSettingsCustomizationPrivate::bBlendViewSettingsCustomizationRegistered = true;
}

void UnregisterBlendViewSettingsCustomization()
{
	if (!BlendViewSettingsCustomizationPrivate::bBlendViewSettingsCustomizationRegistered ||
		!FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		BlendViewSettingsCustomizationPrivate::bBlendViewSettingsCustomizationRegistered = false;
		return;
	}

	FPropertyEditorModule& PropertyModule =
		FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.UnregisterCustomClassLayout(UBlendViewSettings::StaticClass()->GetFName());
	PropertyModule.NotifyCustomizationModuleChanged();
	BlendViewSettingsCustomizationPrivate::bBlendViewSettingsCustomizationRegistered = false;
}

#undef LOCTEXT_NAMESPACE
