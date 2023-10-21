#include "Settings.hpp"
#include "init.hpp"
#include "HookTools.hpp"
#include "interfaces.hpp"
#include "navparser.hpp"
#include "playerresource.hpp"
#include "localplayer.hpp"
#include "sdk.hpp"
#include "entitycache.hpp"
#include "CaptureLogic.hpp"
#include "PlayerTools.hpp"
#include "Aimbot.hpp"
#include "navparser.hpp"
#include "MiscAimbot.hpp"
#include "Misc.hpp"
#include "NavBot.hpp"

namespace hacks::NavBot
{

static settings::Boolean enabled("navbot.enabled", "false");
static settings::Boolean search_health("navbot.search-health", "false");
static settings::Boolean search_ammo("navbot.search-ammo", "false");
static settings::Boolean stay_near("navbot.stay-near", "false");
static settings::Boolean capture_objectives("navbot.capture-objectives", "false");
static settings::Boolean defend_intel("navbot.defend-intel", "false");
static settings::Boolean roam_defend("navbot.defend-idle", "false");
static settings::Boolean defend_payload("navbot.defend-payload", "false");
static settings::Int melee_range("navbot.primary-only-melee-range", "150");
static settings::Boolean snipe_sentries("navbot.snipe-sentries", "false");
static settings::Boolean primary_only("navbot.primary-only", "true");
static settings::Int force_slot("navbot.force-slot", "0");
static settings::Int repath_delay("navbot.repath-time.delay", "5000");

// Controls the bot parameters like distance from enemy
struct bot_class_config
{
    float max;
    bool prefer_far;
};

constexpr bot_class_config CONFIG_SHORT_RANGE         = { 600.0f, false };
constexpr bot_class_config CONFIG_MID_RANGE           = { 3000.0f, true };
constexpr bot_class_config CONFIG_LONG_RANGE          = { 4000.0f, true };
bot_class_config selected_config                      = CONFIG_MID_RANGE;

static Timer health_cooldown{};
static Timer ammo_cooldown{};

// Should we search health at all?
bool shouldSearchHealth(bool low_priority = false)
{
    if (!search_health)
        return false;
    // Check if being gradually healed in any way
    if (HasCondition<TFCond_Healing>(LOCAL_E))
        return false;

    // Priority too high
    if (navparser::NavEngine::current_priority > health)
        return false;
    float health_percent = LOCAL_E->m_iHealth() / (float) g_pPlayerResource->GetMaxHealth(LOCAL_E);
    // Get health when below 65%, or below 80% and just patroling
    return health_percent < 0.64f || (low_priority && (navparser::NavEngine::current_priority <= patrol || navparser::NavEngine::current_priority == lowprio_health) && health_percent <= 0.80f);
}

// Should we search ammo at all?
bool shouldSearchAmmo()
{
    if (!search_ammo)
        return false;
    if (CE_BAD(LOCAL_W))
        return false;
    // Priority too high
    if (navparser::NavEngine::current_priority > ammo)
        return false;

    int *weapon_list = (int *) ((uint64_t) (RAW_ENT(LOCAL_E)) + netvar.hMyWeapons);
    if (!weapon_list)
        return false;
    if (g_pLocalPlayer->holding_sniper_rifle && CE_INT(LOCAL_E, netvar.m_iAmmo + 4) <= 5)
        return true;
    for (int i = 0; weapon_list[i]; ++i)
    {
        int handle = weapon_list[i];
        int eid    = HandleToIDX(handle);
        if (eid > MAX_PLAYERS && eid <= HIGHEST_ENTITY)
        {
            IClientEntity *weapon = g_IEntityList->GetClientEntity(eid);
            if (weapon and re::C_BaseCombatWeapon::IsBaseCombatWeapon(weapon) && re::C_TFWeaponBase::UsesPrimaryAmmo(weapon) && !re::C_TFWeaponBase::HasPrimaryAmmo(weapon))
                return true;
        }
    }

    return false;
}

// Get Valid Dispensers (Used for health/ammo)
std::vector<CachedEntity *> getDispensers()
{
    std::vector<CachedEntity *> entities;
    for (auto const &ent : entity_cache::valid_ents)
    {
        if (ent->m_iClassID() != CL_CLASS(CObjectDispenser) || ent->m_iTeam() != g_pLocalPlayer->team)
            continue;
        if (CE_BYTE(ent, netvar.m_bCarryDeploy) || CE_BYTE(ent, netvar.m_bHasSapper) || CE_BYTE(ent, netvar.m_bBuilding))
            continue;

        // This fixes the fact that players can just place dispensers in unreachable locations
        auto local_nav = navparser::NavEngine::findClosestNavSquare(ent->m_vecOrigin());
        if (local_nav->getNearestPoint(ent->m_vecOrigin().AsVector2D()).DistTo(ent->m_vecOrigin()) > 300.0f || local_nav->getNearestPoint(ent->m_vecOrigin().AsVector2D()).z - ent->m_vecOrigin().z > navparser::PLAYER_JUMP_HEIGHT)
            continue;
        entities.push_back(ent);
    }

    // Sort by distance, closer is better
    std::sort(entities.begin(), entities.end(), [](CachedEntity *a, CachedEntity *b) { return a->m_flDistance() < b->m_flDistance(); });
    return entities;
}

// Get entities of given itemtypes (Used for health/ammo)
std::vector<CachedEntity *> getEntities(const std::vector<k_EItemType> &itemtypes)
{
    std::vector<CachedEntity *> entities;
    for (auto const &ent : entity_cache::valid_ents)
    {
        for (auto &itemtype : itemtypes)
        {
            if (ent->m_ItemType() == itemtype)
            {
                entities.push_back(ent);
                break;
            }
        }
    }

    // Sort by distance, closer is better
    std::sort(entities.begin(), entities.end(), [](CachedEntity *a, CachedEntity *b) { return a->m_flDistance() < b->m_flDistance(); });
    return entities;
}

// Find health if needed
bool getHealth(bool low_priority = false)
{
    Priority_list priority = low_priority ? lowprio_health : health;
    if (!health_cooldown.check(*repath_delay / 2))
        return navparser::NavEngine::current_priority == priority;
    if (shouldSearchHealth(low_priority))
    {
        // Already pathing, only try to repath every 2s
        if (navparser::NavEngine::current_priority == priority)
        {
            static Timer repath_timer;
            if (!repath_timer.test_and_set(*repath_delay))
                return true;
        }
        auto healthpacks = getEntities({ ITEM_HEALTH_SMALL, ITEM_HEALTH_MEDIUM, ITEM_HEALTH_LARGE, EDIBLE_SMALL, EDIBLE_MEDIUM, RESUPPLY_LOCKER });
        auto dispensers  = getDispensers();

        auto total_ents = healthpacks;

        // Add dispensers and sort list again
        if (!dispensers.empty())
        {
            total_ents.reserve(healthpacks.size() + dispensers.size());
            total_ents.insert(total_ents.end(), dispensers.begin(), dispensers.end());
            std::sort(total_ents.begin(), total_ents.end(), [](CachedEntity *a, CachedEntity *b) { return a->m_flDistance() < b->m_flDistance(); });
        }

        for (auto healthpack : total_ents)
            // If we succeeed, don't try to path to other packs
            if (navparser::NavEngine::navTo(healthpack->m_vecOrigin(), priority, true, healthpack->m_vecOrigin().DistToSqr(g_pLocalPlayer->v_Origin) > 200.0f * 200.0f))
                return true;
        health_cooldown.update();
    }
    else if (navparser::NavEngine::current_priority == priority)
        navparser::NavEngine::cancelPath();
    return false;
}

static bool was_force = false;
// Find ammo if needed
bool getAmmo(bool force = false)
{
    if (!force && !ammo_cooldown.check(*repath_delay / 2))
        return navparser::NavEngine::current_priority == ammo;
    if (force || shouldSearchAmmo())
    {
        // Already pathing, only try to repath every 2s
        if (navparser::NavEngine::current_priority == ammo)
        {
            static Timer repath_timer;
            if (!repath_timer.test_and_set(*repath_delay))
                return true;
        }
        else
            was_force = false;
        auto ammopacks  = getEntities({ ITEM_AMMO_SMALL, ITEM_AMMO_MEDIUM, ITEM_AMMO_LARGE, RESUPPLY_LOCKER });
        auto dispensers = getDispensers();

        auto total_ents = ammopacks;

        // Add dispensers and sort list again
        if (!dispensers.empty())
        {
            total_ents.reserve(ammopacks.size() + dispensers.size());
            total_ents.insert(total_ents.end(), dispensers.begin(), dispensers.end());
            std::sort(total_ents.begin(), total_ents.end(), [](CachedEntity *a, CachedEntity *b) { return a->m_flDistance() < b->m_flDistance(); });
        }
        for (auto ammopack : total_ents)
            // If we succeeed, don't try to path to other packs
            if (navparser::NavEngine::navTo(ammopack->m_vecOrigin(), ammo, true, ammopack->m_vecOrigin().DistToSqr(g_pLocalPlayer->v_Origin) > 200.0f * 200.0f))
            {
                was_force = force;
                return true;
            }
        ammo_cooldown.update();
    }
    else if (navparser::NavEngine::current_priority == ammo && !was_force)
        navparser::NavEngine::cancelPath();
    return false;
}

// Vector of sniper spot positions we can nav to
std::vector<Vector> sniper_spots;

// Used for time between refreshing sniperspots
static Timer refresh_sniperspots_timer{};
void refreshSniperSpots()
{
    if (!refresh_sniperspots_timer.test_and_set(6000))
        return;

    sniper_spots.clear();

    // Search all nav areas for valid sniper spots
    for (auto &area : navparser::NavEngine::getNavFile()->m_areas)
        for (auto &hiding_spot : area.m_hidingSpots)
            // Spots actually marked for sniping
            if (hiding_spot.IsExposed() || hiding_spot.IsGoodSniperSpot() || hiding_spot.IsIdealSniperSpot())
                sniper_spots.emplace_back(hiding_spot.m_pos);
}

std::pair<CachedEntity *, float> getNearestPlayerDistance()
{
    float distance         = FLT_MAX;
    CachedEntity *best_ent = nullptr;
    for (auto const &ent: entity_cache::player_cache)
    {
        
        if (CE_VALID(ent) && ent->m_vecDormantOrigin() && g_pPlayerResource->IsAlive(ent->m_IDX) && ent->m_bEnemy() && g_pLocalPlayer->v_Origin.DistTo(ent->m_vecOrigin()) < distance && player_tools::shouldTarget(ent) && !IsPlayerInvisible(ent))
        {
            distance = g_pLocalPlayer->v_Origin.DistTo(*ent->m_vecDormantOrigin());
            best_ent = ent;
        }
    }

    return { best_ent, distance };
}

enum slots
{
    primary   = 1,
    secondary = 2,
    melee     = 3,
};

// Check if an area is valid for stay near. the Third parameter is to save some performance.
bool isAreaValidForStayNear(Vector ent_origin, CNavArea *area, bool fix_local_z = true)
{
    if (fix_local_z)
        ent_origin.z += navparser::PLAYER_JUMP_HEIGHT;
    auto area_origin = area->m_center;
    area_origin.z += navparser::PLAYER_JUMP_HEIGHT;

    // Do all the distance checks
    float distance = ent_origin.DistToSqr(area_origin);

    // Blacklisted
    if (navparser::NavEngine::getFreeBlacklist()->find(area) != navparser::NavEngine::getFreeBlacklist()->end())
        return false;
    // Too far away
    if (distance > selected_config.max * selected_config.max)
        return false;
    // Attempt to vischeck
    if (!IsVectorVisibleNavigation(ent_origin, area_origin))
        return false;

    return true;
}

// Actual logic, used to de-duplicate code
bool stayNearTarget(CachedEntity *ent)
{
    auto ent_origin = ent->m_vecDormantOrigin();
    // No origin recorded, don't bother
    if (!ent_origin)
        return false;

    // Add the vischeck height
    ent_origin->z += navparser::PLAYER_JUMP_HEIGHT;

    // Use std::pair to avoid using the distance functions more than once
    std::vector<std::pair<CNavArea *, float>> good_areas{};

    for (auto &area : navparser::NavEngine::getNavFile()->m_areas)
    {
        auto area_origin = area.m_center;

        // Is this area valid for stay near purposes?
        if (!isAreaValidForStayNear(*ent_origin, &area, false))
            continue;

        float distance = (*ent_origin).DistToSqr(area_origin);
        // Good area found
        good_areas.emplace_back(&area, distance);
    }
    // Sort based on distance
    if (selected_config.prefer_far)
        std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second > b.second; });
    else
        std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second < b.second; });

    // Try to path to all the good areas, based on distance
    if (std::ranges::any_of(good_areas, [](std::pair<CNavArea *, float> area) { return navparser::NavEngine::navTo(area.first->m_center, staynear, true, !navparser::NavEngine::isPathing()); }))
        return true;

    return false;
}

// A bunch of basic checks to ensure we don't try to target an invalid entity
bool isStayNearTargetValid(CachedEntity *ent)
{
    return CE_VALID(ent) && g_pPlayerResource->IsAlive(ent->m_IDX) && ent->m_IDX != g_pLocalPlayer->entity_idx && g_pLocalPlayer->team != ent->m_iTeam() && player_tools::shouldTarget(ent) && !IsPlayerInvisible(ent) && !IsPlayerInvulnerable(ent);
}

// Try to stay near enemies and stalk them (or in case of sniper, try to stay far from them
// and snipe them)
bool stayNear()
{
    PROF_SECTION(stayNear)
    static Timer staynear_cooldown{};
    static CachedEntity *previous_target = nullptr;

    // Stay near is expensive so we have to cache. We achieve this by only checking a pre-determined amount of players every
    // CreateMove
    constexpr int MAX_STAYNEAR_CHECKS_RANGE = 3;
    constexpr int MAX_STAYNEAR_CHECKS_CLOSE = 2;
    static int lowest_check_index           = 0;

    // Stay near is off
    if (!stay_near)
        return false;
    // Don't constantly path, it's slow.
    // Far range classes do not need to repath nearly as often as close range ones.
    if (!staynear_cooldown.test_and_set(selected_config.prefer_far ? *repath_delay : *repath_delay))
        return navparser::NavEngine::current_priority == staynear;

    // Too high priority, so don't try
    if (navparser::NavEngine::current_priority > staynear)
        return false;

    // Check and use our previous target if available
    if (isStayNearTargetValid(previous_target))
    {
        auto ent_origin = previous_target->m_vecDormantOrigin();
        if (ent_origin)
        {
            // Check if current target area is valid
            if (navparser::NavEngine::isPathing())
            {
                auto crumbs = navparser::NavEngine::getCrumbs();
                // We cannot just use the last crumb, as it is always nullptr
                if (crumbs->size() > 1)
                {
                    auto last_crumb = (*crumbs)[crumbs->size() - 2];
                    // Area is still valid, stay on it
                    if (isAreaValidForStayNear(*ent_origin, last_crumb.navarea))
                        return true;
                }
            }
            // Else Check our origin for validity (Only for ranged classes)
            else if (selected_config.prefer_far && isAreaValidForStayNear(*ent_origin, navparser::NavEngine::findClosestNavSquare(LOCAL_E->m_vecOrigin())))
                return true;
        }
        // Else we try to path again
        if (stayNearTarget(previous_target))
            return true;
        // Failed, invalidate previous target and try others
        previous_target = nullptr;
    }

    auto advance_count = selected_config.prefer_far ? MAX_STAYNEAR_CHECKS_RANGE : MAX_STAYNEAR_CHECKS_CLOSE;

    // Ensure it is in bounds and also wrap around
    if (lowest_check_index > g_IEngine->GetMaxClients())
        lowest_check_index = 0;

    int calls = 0;
    // Test all entities
    for (int i = lowest_check_index; i <= g_IEngine->GetMaxClients(); ++i)
    {
        CachedEntity* ent = ENTITY(i);
        if (calls >= advance_count)
            break;
        calls++;
        lowest_check_index++;
        
        if (!isStayNearTargetValid(ent))
        {
            calls--;
            continue;
        }
        // Succeeded pathing
        if (stayNearTarget(ent))
        {
            previous_target = ent;
            return true;
        }
    }
    // Stay near failed to find any good targets, add extra delay
    staynear_cooldown.last += std::chrono::seconds(10);
    return false;
}

bool isVisible;

bool meleeAttack(int slot, std::pair<CachedEntity *, float> &nearest)
{
    // Check if the nearest enemy is invulnerable
    if (nearest.first && IsPlayerInvulnerable(nearest.first))
    {
        // If the nearest enemy is invulnerable, don't engage or follow
        return false;
    }

    // There is no point in engaging the melee AI if we are not using melee
    if (slot != melee || !nearest.first)
    {
        if (navparser::NavEngine::current_priority == prio_melee)
            navparser::NavEngine::cancelPath();
        return false;
    }

    // Too high priority, so don't try
    if (navparser::NavEngine::current_priority > prio_melee)
        return false;

    auto raw_local = RAW_ENT(LOCAL_E);

    // We are charging, let the charge aimbot do its job
    if (HasCondition<TFCond_Charging>(LOCAL_E))
    {
        navparser::NavEngine::cancelPath();
        return true;
    }

    static Timer melee_cooldown{};

    {
        Ray_t ray;
        trace_t trace;
        trace::filter_default.SetSelf(raw_local);

        auto hb = nearest.first->hitboxes.GetHitbox(spine_1);
        if (hb)
        {
            ray.Init(g_pLocalPlayer->v_Origin + Vector{ 0, 0, 20 }, hb->center, raw_local->GetCollideable()->OBBMins(), raw_local->GetCollideable()->OBBMaxs());
            g_ITrace->TraceRay(ray, MASK_PLAYERSOLID, &trace::filter_default, &trace);
            hacks::NavBot::isVisible = (IClientEntity *)trace.m_pEnt == RAW_ENT(nearest.first);
        }
        else
            hacks::NavBot::isVisible = false;
    }
    // If we are close enough and the enemy is visible, don't even bother with using the navparser to get there
    if (nearest.second < 400 && hacks::NavBot::isVisible)
    {
        WalkTo(nearest.first->m_vecOrigin());
        navparser::NavEngine::cancelPath();
        return true;
    }
    else
    {
        // Don't constantly path, it's slow.
        // The closer we are, the more we should try to path
        if (!melee_cooldown.test_and_set(nearest.second < 200 ? 200 : nearest.second < 1000 ? 500 : 2000) && navparser::NavEngine::isPathing())
            return navparser::NavEngine::current_priority == prio_melee;

        // Just walk at the enemy l0l
        if (navparser::NavEngine::navTo(nearest.first->m_vecOrigin(), prio_melee, true, !navparser::NavEngine::isPathing()))
            return true;

        return false;
    }
}


// Basically the same as isAreaValidForStayNear, but some restrictions lifted.
bool isAreaValidForSnipe(Vector ent_origin, Vector area_origin, bool fix_sentry_z = true)
{
    if (fix_sentry_z)
        ent_origin.z += 40.0f;
    area_origin.z += navparser::PLAYER_JUMP_HEIGHT;

    float distance = ent_origin.DistToSqr(area_origin);
    // Too close to be valid
    if (distance <= (1100.0f + navparser::HALF_PLAYER_WIDTH) * (1100.0f + navparser::HALF_PLAYER_WIDTH))
        return false;
    // Fails vischeck, bad
    if (!IsVectorVisibleNavigation(area_origin, ent_origin))
        return false;

    return true;
}

// Try to snipe the sentry
bool tryToSnipe(CachedEntity *ent)
{
    auto ent_origin = GetBuildingPosition(ent);
    // Add some z to dormant sentries as it only returns origin
    if (CE_BAD(ent))
        ent_origin.z += 40.0f;

    std::vector<std::pair<CNavArea *, float>> good_areas;
    for (auto &area : navparser::NavEngine::getNavFile()->m_areas)
    {
        // Not usable
        if (!isAreaValidForSnipe(ent_origin, area.m_center, false))
            continue;
        good_areas.push_back(std::pair<CNavArea *, float>(&area, area.m_center.DistToSqr(ent_origin)));
    }

    // Sort based on distance
    if (selected_config.prefer_far)
        std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second > b.second; });
    else
        std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second < b.second; });

    if (std::ranges::any_of(good_areas, [](std::pair<CNavArea *, float> area) { return navparser::NavEngine::navTo(area.first->m_center, snipe_sentry); }))
        return true;

    return false;
}

// Is our target valid?
bool isSnipeTargetValid(CachedEntity *ent)
{
    return CE_VALID(ent) && ent->m_bAlivePlayer() && ent->m_iTeam() != g_pLocalPlayer->team && ent->m_iClassID() == CL_CLASS(CObjectSentrygun);
}

// Try to Snipe sentries
bool snipeSentries()
{
    static Timer sentry_snipe_cooldown;
    static CachedEntity *previous_target = nullptr;

    if (!*snipe_sentries)
        return false;

    // Sentries don't move often, so we can use a slightly longer timer
    if (!sentry_snipe_cooldown.test_and_set(6000))
        return navparser::NavEngine::current_priority == snipe_sentry || isSnipeTargetValid(previous_target);

    if (isSnipeTargetValid(previous_target))
    {
        auto crumbs = navparser::NavEngine::getCrumbs();
        // We cannot just use the last crumb, as it is always nullptr
        if (crumbs->size() > 1)
        {
            auto last_crumb = (*crumbs)[crumbs->size() - 2];
            // Area is still valid, stay on it
            if (isAreaValidForSnipe(GetBuildingPosition(previous_target), last_crumb.navarea->m_center))
                return true;
        }
        if (tryToSnipe(previous_target))
            return true;
    }

    for (auto const &ent : entity_cache::valid_ents)
    {
        // Invalid sentry
        if (!isSnipeTargetValid(ent))
            continue;
        // Succeeded in trying to snipe it
        if (tryToSnipe(ent))
        {
            previous_target = ent;
            return true;
        }
    }

    return false;
}

enum building
{
    dispenser = 0,
    sentry    = 2
};

enum capture_type
{
    no_capture,
    ctf,
    payload,
    controlpoints
};

static capture_type current_capturetype = no_capture;
// Overwrite to return true for payload carts as an example
static bool overwrite_capture = false;

std::optional<Vector> getCtfGoal(int our_team, int enemy_team)
{
    // Get Flag related information
    auto status   = flagcontroller::getStatus(enemy_team);
    auto position = flagcontroller::getPosition(enemy_team);
    auto carrier  = flagcontroller::getCarrier(enemy_team);

    auto status_friendly   = flagcontroller::getStatus(our_team);
    auto position_friendly = flagcontroller::getPosition(our_team);
    auto carrier_friendly  = flagcontroller::getCarrier(our_team);

    // No flags :(
    if (!position || !position_friendly)
        return std::nullopt;

    current_capturetype = ctf;

    // Defend our intel and help other bots escort enemy intel, this is priority over capturing
    if (defend_intel)
    {
        // Goto the enemy who took our intel - highest priority
        if (status_friendly == TF_FLAGINFO_STOLEN)
        {
            if (player_tools::shouldTargetSteamId(carrier_friendly->player_info.friendsID))
                return carrier_friendly->m_vecDormantOrigin();
        }
        // Standby a dropped friendly flag - medium priority
        if (status_friendly == TF_FLAGINFO_DROPPED)
        {
            // Dont spam nav if we are already there
            if ((*position_friendly).DistTo(LOCAL_E->m_vecOrigin()) < 100.0f)
            {
                overwrite_capture = true;
                return std::nullopt;
            }
            return position_friendly;
        }
        // Assist other bots with capturing - low priority
        if (status == TF_FLAGINFO_STOLEN && carrier != LOCAL_E)
        {
            if (!player_tools::shouldTargetSteamId(carrier->player_info.friendsID))
                return carrier->m_vecDormantOrigin();
        }
    }

    // Flag is taken by us
    if (status == TF_FLAGINFO_STOLEN)
    {
        // CTF is the current capture type.
        if (carrier == LOCAL_E)
        {
            // Return our capture point location
            auto team_flag = flagcontroller::getFlag(our_team);
            return team_flag.spawn_pos;
        }
    }
    // Get the flag if not taken by us already
    else
    {
        return position;
    }

    return std::nullopt;
}

std::optional<Vector> getPayloadGoal(int our_team)
{
    auto position = plcontroller::getClosestPayload(g_pLocalPlayer->v_Origin, our_team);
    // No payloads found :(
    if (!position)
        return std::nullopt;
    current_capturetype = payload;

    // Adjust position so it's not floating high up, provided the local player is close.
    if (LOCAL_E->m_vecOrigin().DistTo(*position) <= 200.0f)
        (*position).z = LOCAL_E->m_vecOrigin().z;
    // If close enough, don't move (mostly due to lifts)
    if ((*position).DistTo(LOCAL_E->m_vecOrigin()) <= 0.0f)
    {
        overwrite_capture = true;
        return std::nullopt;
    }
    else
        return position;
}

std::optional<Vector> getControlPointGoal(int our_team)
{
    auto position = cpcontroller::getClosestControlPoint(g_pLocalPlayer->v_Origin, our_team);
    // No points found :(
    if (!position)
        return std::nullopt;

    current_capturetype = controlpoints;
    // If close enough, don't move
    if ((*position).DistTo(LOCAL_E->m_vecOrigin()) <= 50.0f)
    {
        overwrite_capture = true;
        return std::nullopt;
    }
    else
        return position;
}

// Try to capture objectives
bool captureObjectives()
{
    static Timer capture_timer;
    static Vector previous_target(0.0f);
    
    if (!capture_objectives || !capture_timer.check(1000))
        return false;

    // Priority too high, don't try
    if (navparser::NavEngine::current_priority > capture)
        return false;

    // Where we want to go
    std::optional<Vector> target;

    int our_team   = g_pLocalPlayer->team;
    int enemy_team = our_team == TEAM_BLU ? TEAM_RED : TEAM_BLU;

    current_capturetype = no_capture;
    overwrite_capture   = false;

    // Run ctf logic
    target = getCtfGoal(our_team, enemy_team);
    // Not ctf, run payload
    if (current_capturetype == no_capture)
    {
        target = getPayloadGoal(our_team);
        // Not payload, run control points
        if (current_capturetype == no_capture)
        {
            target = getControlPointGoal(our_team);
        }
    }

    // Overwritten, for example because we are currently on the payload, cancel any sort of pathing and return true
    if (overwrite_capture)
    {
        navparser::NavEngine::cancelPath();
        return true;
    }
    // No target, bail and set on cooldown
    else if (!target)
    {
        capture_timer.update();
        return false;
    }
    // If priority is not capturing or we have a new target, try to path there
    else if (navparser::NavEngine::current_priority != capture || *target != previous_target)
    {
        if (navparser::NavEngine::navTo(*target, capture, true, !navparser::NavEngine::isPathing()))
        {
            previous_target = *target;
            return true;
        }
        else
            capture_timer.update();
    }

    return false;
}

// Roam around map
bool doRoam()
{
    static Timer roam_timer;
    // Don't path constantly
    if (!roam_timer.test_and_set(*repath_delay))
        return false;

    // Defend our objective if possible
    if (roam_defend)
    {
        int enemy_team = g_pLocalPlayer->team == TEAM_BLU ? TEAM_RED : TEAM_BLU;

        std::optional<Vector> target;
        target = getPayloadGoal(enemy_team);
        if (!target)
            target = getControlPointGoal(enemy_team);
        if (target)
        {
            if ((*target).DistTo(g_pLocalPlayer->v_Origin) <= 250.0f)
            {
                navparser::NavEngine::cancelPath();
                return true;
            }
            if (navparser::NavEngine::navTo(*target, patrol))
                return true;
        }
    }

    // Defend our intel and help other bots escort enemy intel, this is priority over capturing
    if (defend_payload)
    {
        int enemy_team = g_pLocalPlayer->team == TEAM_BLU ? TEAM_RED : TEAM_BLU;

        std::optional<Vector> target;
        target = getPayloadGoal(enemy_team);
        if (!target)
            target = getControlPointGoal(enemy_team);
        if (target)
        {
            if ((*target).DistTo(g_pLocalPlayer->v_Origin) <= 250.0f)
            {
                navparser::NavEngine::cancelPath();
                return true;
            }
            if (navparser::NavEngine::navTo(*target, patrol, true, navparser::NavEngine::current_priority != patrol))
                return true;
        }
    }

    // No sniper spots :shrug:
    if (sniper_spots.empty())
        return false;
    // Don't overwrite current roam
    if (navparser::NavEngine::current_priority == patrol)
        return false;
    // Max 2 attempts
    for (int attempts = 0; attempts < 2 && attempts < sniper_spots.size(); ++attempts)
    {
        // Get a random sniper spot
        auto random = select_randomly(sniper_spots.begin(), sniper_spots.end());
        // Try to nav there
        if (navparser::NavEngine::navTo(*random, patrol))
            return true;
    }

    return false;
}

static int slot = primary;

static slots getBestSlot(slots active_slot, std::pair<CachedEntity *, float> &nearest)
{
    if (force_slot)
        return (slots) *force_slot;
    switch (g_pLocalPlayer->clazz)
    {
        case tf_scout:
        {
            if (nearest.second <= 500 && nearest.first->m_iHealth() < 35)
                return secondary;
            else if (nearest.second <= *melee_range && nearest.first->m_bAlivePlayer() && nearest.first->IsVisible())
                return melee;
            else
                return primary;
        }
        case tf_heavy:
        {
            return primary;
            if (!(g_pLocalPlayer->bRevved || g_pLocalPlayer->bRevving) && nearest.second <= 400 && nearest.first->m_iHealth() < 65 && nearest.first->m_bAlivePlayer())
                return secondary;
            else
                return primary;
        }
        case tf_medic:
        {
            if (nearest.second <= *melee_range && nearest.first->m_bAlivePlayer() && nearest.first->IsVisible())
                return melee;
            else
                return secondary;
        }
        case tf_spy:
        {
            if (nearest.second > 200 && active_slot == primary)
                return active_slot;
            else if (nearest.second <= *melee_range && nearest.first->m_bAlivePlayer() && nearest.first->IsVisible())
                return melee;
            else
                return primary;
        }
        case tf_sniper:
        {
            if (nearest.second <= *melee_range && nearest.first->m_bAlivePlayer() && nearest.first->IsVisible())
                return melee;
            else if (nearest.second <= 300)
                return active_slot;
            else
                return primary;
        }
        case tf_demoman:
        {
            if (nearest.second <= *melee_range && nearest.first->m_bAlivePlayer())
                return melee;
            else if (nearest.second <= 300)
                return secondary;
        }
        case tf_pyro:
        {
            if (nearest.second <= *melee_range && nearest.first->m_bAlivePlayer() && nearest.first->IsVisible())
                return melee;
            else if (nearest.second > 600)
                return secondary;
            else
                return primary;
        }
        case tf_soldier:
        {
            if (nearest.second <= 200 && nearest.first->m_bAlivePlayer())
                return secondary;
            else if (nearest.second <= 300 && nearest.first->m_bAlivePlayer())
                return active_slot;
            else
                return primary;
        }
        case tf_engineer:
        {
            if (nearest.second <= 500)
                return primary;
            else
                return primary;
        }
        default:
        {
            if (nearest.second <= 1000)
                return secondary;
            else if (nearest.second <= 700)
                return active_slot;
            else
                return primary;
        }
    }
}

static void updateSlot(std::pair<CachedEntity*, float> &nearest)
{
    static Timer slot_timer{};
    if ((!force_slot && !primary_only) || !slot_timer.test_and_set(0))
        return;
    if (CE_GOOD(LOCAL_E) && !HasCondition<TFCond_HalloweenGhostMode>(LOCAL_E) && CE_GOOD(LOCAL_W) && LOCAL_E->m_bAlivePlayer())
    {
        IClientEntity *weapon = RAW_ENT(LOCAL_W);
        if (re::C_BaseCombatWeapon::IsBaseCombatWeapon(weapon))
        {
            slot        = re::C_BaseCombatWeapon::GetSlot(weapon) + 1;
            int newslot = getBestSlot(static_cast<slots>(slot), nearest);
            if (slot != newslot)
                g_IEngine->ClientCmd_Unrestricted(format("slot", newslot).c_str());
        }
    }
}

void CreateMove()
{
    if (!enabled || !navparser::NavEngine::isReady())
        return;
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || HasCondition<TFCond_HalloweenGhostMode>(LOCAL_E))
        return;

    refreshSniperSpots();

    // Update the distance config
    switch (g_pLocalPlayer->clazz)
    {
    case tf_scout:
    case tf_heavy:
        selected_config = CONFIG_SHORT_RANGE;
        break;
    case tf_engineer:
        selected_config = CONFIG_SHORT_RANGE;
        break;
    case tf_sniper:
        selected_config = CONFIG_LONG_RANGE;
        break;
    default:
        selected_config = CONFIG_MID_RANGE;
    }

    auto nearest = getNearestPlayerDistance();

    updateSlot(nearest);
    // hitting people is MAIN/first priority
    if (meleeAttack(slot, nearest))
        return;
    // Second priority should be getting health
    else if (getHealth())
        return;
    // If we aren't getting health, get ammo
    else if (getAmmo())
        return;
    // Try to capture objectives
    else if (captureObjectives())
        return;
    // Try to snipe sentries
    else if (snipeSentries())
        return;
    // Try to stalk enemies
    else if (stayNear())
        return;
    // Try to get health with a lower prioritiy
    else if (getHealth(true))
        return;
    // We have nothing else to do, roam
    else if (doRoam())
        return;
}

void LevelInit()
{
    // Make it run asap
    refresh_sniperspots_timer.last -= std::chrono::seconds(10);
    sniper_spots.clear();
}

#if ENABLE_VISUALS
void Draw()
{

}
#endif

static InitRoutine init(
    []()
    {
        EC::Register(EC::CreateMove, CreateMove, "navbot_cm");
        EC::Register(EC::CreateMoveWarp, CreateMove, "navbot_cm");
        EC::Register(EC::LevelInit, LevelInit, "navbot_levelinit");
#if ENABLE_VISUALS
        EC::Register(EC::Draw, Draw, "navbot_draw");
#endif
        LevelInit();
    });

} // namespace hacks::NavBot