// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FBlendViewGraphCommands final : public TCommands<FBlendViewGraphCommands>
{
public:
	FBlendViewGraphCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> BeginTranslate;
	TSharedPtr<FUICommandInfo> BeginRotate;
	TSharedPtr<FUICommandInfo> BeginScale;
	TSharedPtr<FUICommandInfo> DuplicateAndTranslate;
	TSharedPtr<FUICommandInfo> DeleteAndReconnect;
};
