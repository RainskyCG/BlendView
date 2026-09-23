// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BlendViewInputTypes.h"
#include "Framework/Commands/Commands.h"

class FBlendViewCommands final : public TCommands<FBlendViewCommands>
{
public:
	FBlendViewCommands();

	virtual void RegisterCommands() override;
	static bool MatchesInputEvent(const FBlendViewInputEvent& Event, const FUICommandInfo& Command);

	TSharedPtr<FUICommandInfo> ToggleBlendView;
	TSharedPtr<FUICommandInfo> BeginTranslate;
	TSharedPtr<FUICommandInfo> BeginRotate;
	TSharedPtr<FUICommandInfo> BeginScale;
	TSharedPtr<FUICommandInfo> BeginMirror;
	TSharedPtr<FUICommandInfo> TogglePivotEditMode;
	TSharedPtr<FUICommandInfo> SetSnapBase;
	TSharedPtr<FUICommandInfo> ClearTransformConstraint;
	TSharedPtr<FUICommandInfo> ToggleTransformStatusBar;
	TSharedPtr<FUICommandInfo> OpenQuickFavorites;
	TSharedPtr<FUICommandInfo> OpenCommandSearch;
	TSharedPtr<FUICommandInfo> FrameSelected;
	TSharedPtr<FUICommandInfo> MoveSelectedToFolder;
	TSharedPtr<FUICommandInfo> DuplicateAndTranslate;
};
