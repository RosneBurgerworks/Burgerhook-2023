#pragma once

#include <array>
#include <stdint.h>

namespace hacks::NavBot
{
extern bool isVisible;
std::pair<CachedEntity *, float> getNearestPlayerDistance();
} // namespace hacks::NavBot

