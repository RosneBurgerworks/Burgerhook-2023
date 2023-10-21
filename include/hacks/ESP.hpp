#pragma once

#include "config.h"
#if ENABLE_VISUALS
#include "colors.hpp"

namespace hacks::esp
{
// Init
void Shutdown();
// Strings
void SetEntityColor(CachedEntity *entity, const rgba_t &color);
} // namespace hacks::esp
#endif
