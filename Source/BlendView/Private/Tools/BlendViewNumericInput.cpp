// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewNumericInput.h"

bool FBlendViewNumericInput::HandleKey(const FKey& Key)
{
	TCHAR Character = '\0';
	if (Key == EKeys::Zero || Key == EKeys::NumPadZero) Character = '0';
	else if (Key == EKeys::One || Key == EKeys::NumPadOne) Character = '1';
	else if (Key == EKeys::Two || Key == EKeys::NumPadTwo) Character = '2';
	else if (Key == EKeys::Three || Key == EKeys::NumPadThree) Character = '3';
	else if (Key == EKeys::Four || Key == EKeys::NumPadFour) Character = '4';
	else if (Key == EKeys::Five || Key == EKeys::NumPadFive) Character = '5';
	else if (Key == EKeys::Six || Key == EKeys::NumPadSix) Character = '6';
	else if (Key == EKeys::Seven || Key == EKeys::NumPadSeven) Character = '7';
	else if (Key == EKeys::Eight || Key == EKeys::NumPadEight) Character = '8';
	else if (Key == EKeys::Nine || Key == EKeys::NumPadNine) Character = '9';
	else if (Key == EKeys::Period || Key == EKeys::Decimal) Character = '.';
	else if (Key == EKeys::Hyphen || Key == EKeys::Subtract) Character = '-';
	else return false;

	bActive = true;
	if (Character == '-')
	{
		if (Buffer.StartsWith(TEXT("-")))
		{
			Buffer.RightChopInline(1);
		}
		else
		{
			Buffer = TEXT("-") + Buffer;
		}
	}
	else if (Character == '.')
	{
		if (!Buffer.Contains(TEXT(".")))
		{
			if (Buffer.IsEmpty() || Buffer == TEXT("-"))
			{
				Buffer += TEXT("0");
			}
			Buffer.AppendChar(Character);
		}
	}
	else
	{
		Buffer.AppendChar(Character);
	}

	return true;
}

EBlendViewNumericBackspaceResult FBlendViewNumericInput::HandleBackspace()
{
	if (!bActive)
	{
		return EBlendViewNumericBackspaceResult::Ignored;
	}

	if (!Buffer.IsEmpty())
	{
		Buffer.LeftChopInline(1);
	}
	if (Buffer.IsEmpty())
	{
		Clear();
		return EBlendViewNumericBackspaceResult::Cleared;
	}
	return EBlendViewNumericBackspaceResult::Updated;
}

void FBlendViewNumericInput::Clear()
{
	bActive = false;
	Buffer.Reset();
}

double FBlendViewNumericInput::GetValue() const
{
	return Buffer.IsEmpty() || Buffer == TEXT("-")
		? 0.0
		: FCString::Atod(*Buffer);
}
