// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#if defined(JAVELIN_ENABLE_PROFILING)
#include "tracy/Tracy.hpp"
#define JAVELIN_ZONE_SCOPED ZoneScoped
#define JAVELIN_ZONE_SCOPED_N(name) ZoneScopedN(name)
#define JAVELIN_ZONE_TEXTF(...) ZoneTextF(__VA_ARGS__)
#define JAVELIN_FRAME_MARK FrameMark
#else
#define JAVELIN_ZONE_SCOPED
#define JAVELIN_ZONE_SCOPED_N(name)
#define JAVELIN_ZONE_TEXTF(...)
#define JAVELIN_FRAME_MARK
#endif
