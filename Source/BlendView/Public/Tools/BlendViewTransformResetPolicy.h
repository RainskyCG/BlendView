// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/BlendViewTransformTypes.h"

class FBlendViewTransformResetPolicy final
{
public:
	static TOptional<EBlendViewTransformMode> GetFallbackMode(EBlendViewTransformResetChannel Channel)
	{
		switch (Channel)
		{
		case EBlendViewTransformResetChannel::Location:
			return EBlendViewTransformMode::Translate;
		case EBlendViewTransformResetChannel::Rotation:
			return EBlendViewTransformMode::Rotate;
		case EBlendViewTransformResetChannel::Scale:
			return EBlendViewTransformMode::Scale;
		default:
			return TOptional<EBlendViewTransformMode>();
		}
	}

	static bool DoesChannelMatchMode(
		EBlendViewTransformResetChannel Channel,
		EBlendViewTransformMode Mode)
	{
		const TOptional<EBlendViewTransformMode> FallbackMode = GetFallbackMode(Channel);
		return FallbackMode.IsSet() && FallbackMode.GetValue() == Mode;
	}
};
