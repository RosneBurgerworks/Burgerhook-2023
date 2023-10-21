#pragma once
#include "config.h"
#include "timer.hpp"
namespace hacks::autojoin
{
void updateSearch();
void onShutdown();
#if !ENABLE_VISUALS
extern Timer queue_time;
#endif
} // namespace hacks::autojoin
