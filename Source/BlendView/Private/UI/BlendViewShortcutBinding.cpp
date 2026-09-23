// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewShortcutBinding.h"

#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/InputChord.h"
#include "Localization/BlendViewLocalization.h"
#include "Misc/ConfigCacheIni.h"

namespace BlendViewShortcutBinding
{
	bool HasCustomPrimaryShortcut(const TSharedPtr<FUICommandInfo>& CommandInfo)
	{
		if (!CommandInfo.IsValid())
		{
			return false;
		}

		const FInputChord ActiveChord = *CommandInfo->GetActiveChord(EMultipleKeyBindingIndex::Primary);
		const FInputChord& DefaultChord = CommandInfo->GetDefaultChord(EMultipleKeyBindingIndex::Primary);
		return ActiveChord != DefaultChord;
	}

	void SaveEditorInputBindings()
	{
		FInputBindingManager::Get().SaveInputBindings();
		if (GConfig)
		{
			GConfig->Flush(false, GEditorKeyBindingsIni);
		}
	}

	TSharedPtr<FUICommandInfo> FindConflictingShortcutCommand(
		const TSharedRef<FUICommandInfo>& CommandInfo,
		const FInputChord& NewChord)
	{
		if (!NewChord.IsValidChord())
		{
			return nullptr;
		}

		constexpr bool bCheckDefaultChord = false;
		const TSharedPtr<FUICommandInfo> FoundCommand =
			FInputBindingManager::Get().GetCommandInfoFromInputChord(
				CommandInfo->GetBindingContext(),
				NewChord,
				bCheckDefaultChord);

		return FoundCommand.IsValid() && FoundCommand->GetCommandName() != CommandInfo->GetCommandName()
			? FoundCommand
			: nullptr;
	}

	bool AssignPrimaryShortcut(
		const TSharedRef<FUICommandInfo>& CommandInfo,
		const FInputChord& NewChord,
		FText& OutErrorText)
	{
		if (!NewChord.IsValidChord())
		{
			return false;
		}

		if (const TSharedPtr<FUICommandInfo> ConflictCommand =
			FindConflictingShortcutCommand(CommandInfo, NewChord))
		{
			OutErrorText = FText::Format(
				FBlendViewLocalization::Text(
					TEXT("{0} 已绑定到 {1}"),
					TEXT("{0} is already bound to {1}")),
				NewChord.GetInputText(),
				ConflictCommand->GetLabel());
			return false;
		}

		CommandInfo->SetActiveChord(NewChord, EMultipleKeyBindingIndex::Primary);
		SaveEditorInputBindings();
		OutErrorText = FText::GetEmpty();
		return true;
	}

	void ResetPrimaryShortcut(const TSharedRef<FUICommandInfo>& CommandInfo)
	{
		const FInputChord& DefaultChord = CommandInfo->GetDefaultChord(EMultipleKeyBindingIndex::Primary);
		if (DefaultChord.IsValidChord())
		{
			CommandInfo->SetActiveChord(DefaultChord, EMultipleKeyBindingIndex::Primary);
		}
		else
		{
			CommandInfo->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
		}
		SaveEditorInputBindings();
	}
}
