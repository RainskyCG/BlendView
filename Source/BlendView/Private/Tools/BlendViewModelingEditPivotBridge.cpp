// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewModelingEditPivotBridge.h"
#include "Compat/BlendViewEditorModeToolsCompat.h"

#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/TransformProxy.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorSupportDelegates.h"
#include "InteractiveTool.h"
#include "InteractiveToolManager.h"
#include "Tools/UEdMode.h"
#include "UObject/UnrealType.h"
#include "UnrealEdGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlendViewModelingEditPivotBridge, Log, All);

namespace
{
	const FEditorModeID ModelingToolsEditorModeId(TEXT("EM_ModelingToolsEditorMode"));
	const FString EditPivotToolIdentifier(TEXT("BeginEditPivotTool"));

	TWeakObjectPtr<UTransformProxy> CachedEditPivotProxy;
	FTransform CachedInitialEditPivotTransform = FTransform::Identity;

	UEdMode* GetActiveModelingMode()
	{
		if (!BlendViewEditorModeToolsCompat::IsLevelEditorModeToolsAvailable())
		{
			return nullptr;
		}
		return GLevelEditorModeTools().GetActiveScriptableMode(ModelingToolsEditorModeId);
	}

	UInteractiveToolManager* GetActiveModelingToolManager()
	{
		if (UEdMode* ModelingMode = GetActiveModelingMode())
		{
			return ModelingMode->GetToolManager(EToolsContextScope::EdMode);
		}
		return nullptr;
	}

	bool ResolveTarget(
		UInteractiveTool* ActiveTool,
		FBlendViewModelingEditPivotBridge::FTarget& OutTarget)
	{
		OutTarget = FBlendViewModelingEditPivotBridge::FTarget();
		if (!ActiveTool)
		{
			return false;
		}

		FProperty* ActiveGizmosProperty = ActiveTool->GetClass()->FindPropertyByName(TEXT("ActiveGizmos"));
		FArrayProperty* ActiveGizmosArrayProperty = CastField<FArrayProperty>(ActiveGizmosProperty);
		FStructProperty* GizmoStructProperty = ActiveGizmosArrayProperty
			? CastField<FStructProperty>(ActiveGizmosArrayProperty->Inner)
			: nullptr;
		if (!ActiveGizmosArrayProperty || !GizmoStructProperty)
		{
			return false;
		}

		void* ArrayValue = ActiveGizmosArrayProperty->ContainerPtrToValuePtr<void>(ActiveTool);
		FScriptArrayHelper ArrayHelper(ActiveGizmosArrayProperty, ArrayValue);
		if (ArrayHelper.Num() < 1)
		{
			return false;
		}

		void* FirstGizmo = ArrayHelper.GetRawPtr(0);
		FProperty* TransformProxyProperty = GizmoStructProperty->Struct->FindPropertyByName(TEXT("TransformProxy"));
		FObjectPropertyBase* TransformProxyObjectProperty = CastField<FObjectPropertyBase>(TransformProxyProperty);
		FProperty* TransformGizmoProperty = GizmoStructProperty->Struct->FindPropertyByName(TEXT("TransformGizmo"));
		FObjectPropertyBase* TransformGizmoObjectProperty = CastField<FObjectPropertyBase>(TransformGizmoProperty);
		if (!TransformProxyObjectProperty || !TransformGizmoObjectProperty)
		{
			return false;
		}

		OutTarget.TransformProxy = Cast<UTransformProxy>(
			TransformProxyObjectProperty->GetObjectPropertyValue_InContainer(FirstGizmo));
		OutTarget.TransformGizmo = Cast<UCombinedTransformGizmo>(
			TransformGizmoObjectProperty->GetObjectPropertyValue_InContainer(FirstGizmo));
		return OutTarget.TransformProxy && OutTarget.TransformGizmo;
	}

	void RedrawEditPivotViewports()
	{
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
		FEditorSupportDelegates::UpdateUI.Broadcast();
	}
}

bool FBlendViewModelingEditPivotBridge::FindActiveTarget(FTarget& OutTarget)
{
	OutTarget = FTarget();
	UInteractiveToolManager* ToolManager = GetActiveModelingToolManager();
	if (!ToolManager)
	{
		return false;
	}

	if (ResolveTarget(ToolManager->GetActiveTool(EToolSide::Mouse), OutTarget))
	{
		return true;
	}
	return ResolveTarget(ToolManager->GetActiveTool(EToolSide::Left), OutTarget);
}

UTransformProxy* FBlendViewModelingEditPivotBridge::FindActiveTransformProxy()
{
	FTarget Target;
	FindActiveTarget(Target);
	return Target.TransformProxy;
}

bool FBlendViewModelingEditPivotBridge::StartToolIfNeeded()
{
	if (!BlendViewEditorModeToolsCompat::IsLevelEditorModeToolsAvailable())
	{
		return false;
	}

	if (!GetActiveModelingMode())
	{
		GLevelEditorModeTools().ActivateMode(ModelingToolsEditorModeId);
	}

	UInteractiveToolManager* ToolManager = GetActiveModelingToolManager();
	if (!ToolManager)
	{
		return false;
	}

	if (FindActiveTransformProxy())
	{
		return true;
	}

	if (ToolManager->HasAnyActiveTool())
	{
		return false;
	}

	if (ToolManager->CanActivateTool(EToolSide::Mouse, EditPivotToolIdentifier) &&
		ToolManager->SelectActiveToolType(EToolSide::Mouse, EditPivotToolIdentifier) &&
		ToolManager->ActivateTool(EToolSide::Mouse))
	{
		return true;
	}

	return ToolManager->CanActivateTool(EToolSide::Left, EditPivotToolIdentifier) &&
		ToolManager->SelectActiveToolType(EToolSide::Left, EditPivotToolIdentifier) &&
		ToolManager->ActivateTool(EToolSide::Left);
}

void FBlendViewModelingEditPivotBridge::CacheInitialTransform(UTransformProxy* TransformProxy)
{
	if (!TransformProxy)
	{
		CachedEditPivotProxy.Reset();
		CachedInitialEditPivotTransform = FTransform::Identity;
		return;
	}

	CachedEditPivotProxy = TransformProxy;
	CachedInitialEditPivotTransform = TransformProxy->GetTransform();
}

void FBlendViewModelingEditPivotBridge::EnsureInitialTransformCached(UTransformProxy* TransformProxy)
{
	if (TransformProxy && CachedEditPivotProxy.Get() != TransformProxy)
	{
		CacheInitialTransform(TransformProxy);
	}
}

bool FBlendViewModelingEditPivotBridge::ResetActiveLocation()
{
	FTarget Target;
	if (!FindActiveTarget(Target))
	{
		return false;
	}

	EnsureInitialTransformCached(Target.TransformProxy);
	FTransform NewTransform = Target.TransformProxy->GetTransform();
	NewTransform.SetLocation(CachedInitialEditPivotTransform.GetLocation());
	if (NewTransform.ContainsNaN())
	{
		return false;
	}

	Target.TransformProxy->bSetPivotMode = true;
	Target.TransformGizmo->SetNewGizmoTransform(NewTransform);
	RedrawEditPivotViewports();
	UE_LOG(LogBlendViewModelingEditPivotBridge, Verbose, TEXT("Edit Pivot reset location."));
	return true;
}

bool FBlendViewModelingEditPivotBridge::ResetActiveRotation()
{
	FTarget Target;
	if (!FindActiveTarget(Target))
	{
		return false;
	}

	EnsureInitialTransformCached(Target.TransformProxy);
	FTransform NewTransform = Target.TransformProxy->GetTransform();
	NewTransform.SetRotation(CachedInitialEditPivotTransform.GetRotation().GetNormalized());
	if (NewTransform.ContainsNaN())
	{
		return false;
	}

	Target.TransformProxy->bSetPivotMode = true;
	Target.TransformGizmo->SetNewGizmoTransform(NewTransform);
	RedrawEditPivotViewports();
	UE_LOG(LogBlendViewModelingEditPivotBridge, Verbose, TEXT("Edit Pivot reset rotation."));
	return true;
}
