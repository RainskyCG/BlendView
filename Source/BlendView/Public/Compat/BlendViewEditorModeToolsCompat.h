// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "Compat/BlendViewEngineVersion.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Modules/ModuleManager.h"
#include "UnrealEdGlobals.h"

namespace BlendViewEditorModeToolsCompat
{
	inline bool IsLevelEditorModeToolsAvailable()
	{
#if BLENDVIEW_UE_5_8_OR_LATER
		return GEditor != nullptr && FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"));
#else
		return GLevelEditorModeToolsIsValid();
#endif
	}
}