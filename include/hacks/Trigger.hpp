#pragma once

#include "common.hpp"
#include "Backtrack.hpp"

namespace hacks::triggerbot
{
bool ShouldShoot();
bool IsTargetStateGood(CachedEntity *entity, std::optional<backtrack::BacktrackData> bt_data = std::nullopt);
CachedEntity *FindEntInSight(float range, bool no_players = false);
bool HeadPreferable(CachedEntity *target);
bool UpdateAimkey();
void Draw();
bool CheckLineBox(Vector B1, Vector B2, Vector L1, Vector L2, Vector &Hit);
} // namespace hacks::triggerbot
