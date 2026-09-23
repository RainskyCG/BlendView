// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#define BLENDVIEW_UE_VERSION_AT_LEAST(Major, Minor) \
	((ENGINE_MAJOR_VERSION > (Major)) || \
		(ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))

#define BLENDVIEW_UE_5_7_OR_LATER BLENDVIEW_UE_VERSION_AT_LEAST(5, 7)
#define BLENDVIEW_UE_5_8_OR_LATER BLENDVIEW_UE_VERSION_AT_LEAST(5, 8)
