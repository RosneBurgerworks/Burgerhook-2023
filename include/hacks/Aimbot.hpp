#pragma once

#include "common.hpp"

class ConVar;
class IClientEntity;

namespace hacks::aimbot
{
extern unsigned last_target_ignore_timer;
// Used to store aimbot data to prevent calculating it again
// Functions used to calculate aimbot data, and if already calculated use it
Vector PredictEntity(CachedEntity *entity, bool vischeck);
bool VischeckPredictedEntity(CachedEntity *entity);

// Functions called by other functions for when certian game calls are run
void Reset();

// Stuff to make storing functions easy
bool isAiming();
CachedEntity *CurrentTarget();
bool ShouldAim();
CachedEntity *RetrieveBestTarget(bool aimkey_state);
bool IsTargetStateGood(CachedEntity *entity);
void Aim(CachedEntity *entity);
void DoAutoshoot(CachedEntity *target = nullptr);
bool smallBoxChecker(CachedEntity *target_entity);
int notVisibleHitbox(CachedEntity *target, int preferred);
std::vector<Vector> GetHitpointsVischeck(CachedEntity *ent, int hitbox);
int autoHitbox(CachedEntity *target);
bool hitscanSpecialCases(CachedEntity *target_entity, int weapon_case);
int BestHitbox(CachedEntity *target);
bool IsHitboxMedium(int hitbox);
int ClosestHitbox(CachedEntity *target);
void DoSlowAim(Vector &inputAngle);
bool UpdateAimkey();
} // namespace hacks::aimbot
