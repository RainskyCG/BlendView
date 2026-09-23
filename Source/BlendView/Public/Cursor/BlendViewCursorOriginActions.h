// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewSceneCursor;

enum class EBlendViewCursorOriginAction : uint8
{
	CursorToOrigin,
	CursorToSelected,
	SelectionToCursorOffset,
	SelectionToCursor,
	OriginToGeometry,
	OriginToCursor,
	OriginToActive,
	OriginToBottom
};

class FBlendViewCursorOriginActions final
{
public:
	static bool Execute(EBlendViewCursorOriginAction Action, FBlendViewSceneCursor& SceneCursor);
	static bool CanExecute(EBlendViewCursorOriginAction Action);
	static const TCHAR* ToLogName(EBlendViewCursorOriginAction Action);
};
