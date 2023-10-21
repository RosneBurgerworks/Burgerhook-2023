#include <hacks/Aimbot.hpp>
#include <hacks/CatBot.hpp>
#include <hacks/AntiAim.hpp>
#include <hacks/ESP.hpp>
#include <hacks/Backtrack.hpp>
#include <PlayerTools.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"
#include "MiscTemporary.hpp"
#include <targethelper.hpp>
#include "hitrate.hpp"
#include "FollowBot.hpp"
#include "Warp.hpp"
#include "NavBot.hpp"

namespace hacks::aimbot
{
static settings::Boolean normal_enable{ "aimbot.enable", "false" };
static settings::Button aimkey{ "aimbot.aimkey.button", "<null>" };
static settings::Int aimkey_mode{ "aimbot.aimkey.mode", "1" };
static settings::Boolean autoshoot{ "aimbot.autoshoot", "true" };
static settings::Boolean autoreload{ "aimbot.autoshoot.activate-heatmaker", "false" };
static settings::Boolean multipoint{ "aimbot.multipoint", "false" };
static settings::Int vischeck_hitboxes{ "aimbot.vischeck-hitboxes", "0" };
static settings::Int hitbox_mode{ "aimbot.hitbox-mode", "0" };
static settings::Float normal_fov{ "aimbot.fov", "0" };
static settings::Int priority_mode{ "aimbot.priority-mode", "0" };
static settings::Boolean wait_for_charge{ "aimbot.wait-for-charge", "false" };

static settings::Boolean silent{ "aimbot.silent", "true" };
static settings::Boolean target_lock{ "aimbot.lock-target", "false" };
static settings::Int hitbox{ "aimbot.hitbox", "0" };
static settings::Boolean zoomed_only{ "aimbot.zoomed-only", "true" };
static settings::Boolean only_can_shoot{ "aimbot.can-shoot-only", "true" };

static settings::Int normal_slow_aim{ "aimbot.slow", "0" };
static settings::Boolean aimbot_debug{ "aimbot.debug", "false" };

static settings::Boolean auto_zoom{ "aimbot.auto.zoom", "false" };
static settings::Boolean auto_unzoom{ "aimbot.auto.unzoom", "false" };
static settings::Int zoom_time("aimbot.auto.zoom.time", "5000");
static settings::Int zoom_distance{ "aimbot.zoom.distance", "1250" };

static settings::Boolean backtrackAimbot{ "aimbot.backtrack", "false" };
static settings::Boolean backtrackLastTickOnly("aimbot.backtrack.only-last-tick", "false");
static bool force_backtrack_aimbot = false;
static settings::Boolean backtrackVischeckAll{ "aimbot.backtrack.vischeck-all", "false" };

static settings::Float max_range{ "aimbot.target.max-range", "4096" };
static settings::Boolean buildings_sentry{ "aimbot.target.sentry", "true" };
static settings::Boolean buildings_other{ "aimbot.target.other-buildings", "true" };
static settings::Boolean npcs{ "aimbot.target.npcs", "false" };
static settings::Boolean stickybot{ "aimbot.target.stickybomb", "false" };
static settings::Int teammates{ "aimbot.target.teammates", "0" };

struct AimbotCalculatedData_s
{
    unsigned long predict_tick{ 0 };
    bool predict_type{ false };
    Vector aim_position{ 0 };
    unsigned long vcheck_tick{ 0 };
    bool visible{ false };
    float fov{ 0 };
    int hitbox{ 0 };
} 

static cd;
int slow_aim;
float fov;
bool enable;

// Used to make rapidfire not knock your enemies out of range
unsigned last_target_ignore_timer = 0;
bool shouldbacktrack_cache = false;

// Func to find value of how far to target ents
inline float EffectiveTargetingRange()
{
    if (GetWeaponMode() == weapon_melee)
        return (float) re::C_TFWeaponBaseMelee::GetSwingRange(RAW_ENT(LOCAL_W));
    return (float) *max_range;
}

inline bool playerTeamCheck(CachedEntity *entity)
{
    return (int) teammates == 2 || (entity->m_bEnemy() && !teammates) || (!entity->m_bEnemy() && teammates) || (CE_GOOD(LOCAL_W) && LOCAL_W->m_iClassID() == CL_CLASS(CTFCrossbow) && entity->m_iHealth() < entity->m_iMaxHealth());
}

// Am I holding Hitman's Heatmaker ?
inline bool CarryingHeatmaker()
{
    return CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 752;
}

// Am I holding the Machina ?
inline bool CarryingMachina()
{
    return CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 526 || CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 30665;
}

// A function to find the best hitbox for a target
inline int BestHitbox(CachedEntity *target)
{
    switch (*hitbox_mode)
    {
        case 0:
            return autoHitbox(target);
        case 1:
            return ClosestHitbox(target);
        case 2:
            return *hitbox;
        default:
            break;
    }
    return -1;
}

inline void UpdateShouldBacktrack()
{
    if (hacks::backtrack::hasData() || !(*backtrackAimbot || force_backtrack_aimbot))
        shouldbacktrack_cache = false;
    else
        shouldbacktrack_cache = true;
}

inline bool shouldBacktrack(CachedEntity *ent)
{
    if (!shouldbacktrack_cache)
        return false;
    else if (ent && ent->m_Type() != ENTITY_PLAYER)
        return false;
    else if (!backtrack::getGoodTicks(ent))
        return false;
    return true;
}
#define GET_MIDDLE(c1, c2) ((corners[c1] + corners[c2]) / 2.0f)

// Get all the valid aim positions
std::vector<Vector> GetValidHitpoints(CachedEntity *ent, int hitbox)
{
    // Recorded vischeckable points
    std::vector<Vector> hitpoints;
    auto hb = ent->hitboxes.GetHitbox(hitbox);

    trace_t trace;
    if (IsEntityVectorVisible(ent, hb->center, true, MASK_SHOT_HULL, &trace, true) && trace.hitbox == hitbox)
        hitpoints.push_back(hb->center);

    // Multipoint
    auto bboxmin = hb->bbox->bbmin;
    auto bboxmax = hb->bbox->bbmax;

    auto transform = ent->hitboxes.GetBones()[hb->bbox->bone];
    QAngle rotation;
    Vector origin;

    MatrixAngles(transform, rotation, origin);

    Vector corners[8];
    GenerateBoxVertices(origin, rotation, bboxmin, bboxmax, corners);

    float shrink_size = 1;

    if (!IsHitboxMedium(hitbox)) // hitbox should be chosen based on size.
        shrink_size = 3;
    else
        shrink_size = 6;

    // Shrink positions by moving towards opposing corner
    for (uint8 i = 0; i < 8; ++i)
        corners[i] += (corners[7 - i] - corners[i]) / shrink_size;

    // Generate middle points on line segments
    // Define cleans up code

    const Vector line_positions[12] = { GET_MIDDLE(0, 1), GET_MIDDLE(0, 2), GET_MIDDLE(1, 3), GET_MIDDLE(2, 3), GET_MIDDLE(7, 6), GET_MIDDLE(7, 5), GET_MIDDLE(6, 4), GET_MIDDLE(5, 4), GET_MIDDLE(0, 4), GET_MIDDLE(1, 5), GET_MIDDLE(2, 6), GET_MIDDLE(3, 7) };

    // Create combined vector
    std::vector<Vector> positions;

    positions.reserve(sizeof(Vector) * 20);
    positions.insert(positions.end(), corners, &corners[8]);
    positions.insert(positions.end(), line_positions, &line_positions[12]);

    for (uint8 i = 0; i < 20; ++i)
    {
        trace_t trace;
        if (IsEntityVectorVisible(ent, positions[i], true, MASK_SHOT_HULL, &trace, true) && trace.hitbox == hitbox)
            hitpoints.push_back(positions[i]);
    }

    if (*vischeck_hitboxes)
    {
        if (*vischeck_hitboxes == 1 && playerlist::AccessData(ent).state != playerlist::k_EState::RAGE)
            return hitpoints;

        int i                  = 0;
        const u_int8_t max_box = ent->hitboxes.GetNumHitboxes();
        while (hitpoints.empty() && i < max_box) // Prevents returning empty at all costs. Loops through every hitbox
        {
            if (hitbox == i)
            {
                ++i;
                continue;
            }
            hitpoints = GetHitpointsVischeck(ent, i);
            ++i;
        }
    }

    return hitpoints;
}

std::vector<Vector> getHitpointsVischeck(CachedEntity *ent, int hitbox)
{
    std::vector<Vector> hitpoints;
    auto hb      = ent->hitboxes.GetHitbox(hitbox);
    auto bboxmin = hb->bbox->bbmin;
    auto bboxmax = hb->bbox->bbmax;

    auto transform = ent->hitboxes.GetBones()[hb->bbox->bone];
    QAngle rotation;
    Vector origin;

    MatrixAngles(transform, rotation, origin);

    Vector corners[8];
    GenerateBoxVertices(origin, rotation, bboxmin, bboxmax, corners);

    float shrink_size = 1;

    if (!IsHitboxMedium(hitbox)) // hitbox should be chosen based on size.
        shrink_size = 3;
    else
        shrink_size = 6;

    // Shrink positions by moving towards opposing corner
    for (uint8 i = 0; i < 8; ++i)
        corners[i] += (corners[7 - i] - corners[i]) / shrink_size;

    // Generate middle points on line segments
    // Define cleans up code
    const Vector line_positions[12] = { GET_MIDDLE(0, 1), GET_MIDDLE(0, 2), GET_MIDDLE(1, 3), GET_MIDDLE(2, 3), GET_MIDDLE(7, 6), GET_MIDDLE(7, 5), GET_MIDDLE(6, 4), GET_MIDDLE(5, 4), GET_MIDDLE(0, 4), GET_MIDDLE(1, 5), GET_MIDDLE(2, 6), GET_MIDDLE(3, 7) };

    // Create combined vector
    std::vector<Vector> positions;

    positions.reserve(sizeof(Vector) * 20);
    positions.insert(positions.end(), corners, &corners[8]);
    positions.insert(positions.end(), line_positions, &line_positions[12]);

    for (uint8 i = 0; i < 20; ++i)
    {
        trace_t trace;
        if (IsEntityVectorVisible(ent, positions[i], true, MASK_SHOT_HULL, &trace, true) && trace.hitbox == hitbox)
            hitpoints.push_back(positions[i]);
    }

    return hitpoints;
}

bool isHitboxMedium(int hitbox)
{
    switch (hitbox)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        return true;
    default:
        return false;
    }
}

// Get the best point to aim at for a given hitbox
std::optional<Vector> GetBestHitpoint(CachedEntity *ent, int hitbox)
{
    auto positions = GetValidHitpoints(ent, hitbox);

    std::optional<Vector> best_pos = std::nullopt;
    float max_score                = FLT_MAX;
    for (auto &position : positions)
    {
        float score = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, position);
        if (score < max_score)
        {
            best_pos  = position;
            max_score = score;
        }
    }

    return best_pos;
}

int GetSentry()
{
    for (int i = 1; i <= HIGHEST_ENTITY; i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_BAD(ent))
            continue;
        else if (ent->m_Type() != ENTITY_BUILDING || ent->m_iClassID() != CL_CLASS(CObjectSentrygun))
            continue;
        else if ((CE_INT(ent, netvar.m_hBuilder) & 0xFFF) != g_pLocalPlayer->entity_idx)
            continue;
        return i;
    }

    return -1;
}

// Reduce Backtrack lag by checking if the ticks hitboxes are within a reasonable FOV range
bool validateTickFOV(backtrack::BacktrackData &tick)
{
    if (fov)
    {
        bool valid_fov = false;
        for (auto &hitbox : tick.hitboxes)
        {
            float score = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, hitbox.center);
            // Check if the FOV is within a 2.0f threshhold
            if (score < fov + 2.0f)
            {
                valid_fov = true;
                break;
            }
        }

        return valid_fov;
    }

    return true;
}

bool AllowNoScope(CachedEntity *target)
{
    if (!target || CarryingMachina())
        return false;

    int target_health       = target->m_iHealth();
    bool carrying_heatmaker = CarryingHeatmaker();

    if (!carrying_heatmaker && target_health <= 50)
        return true;

    if (carrying_heatmaker && target_health <= 40)
        return true;

    bool local_player_crit_boosted = IsPlayerCritBoosted(LOCAL_E);
    if (local_player_crit_boosted && target_health <= 150)
        return true;

    bool local_player_mini_crit_boosted = IsPlayerMiniCritBoosted(LOCAL_E);
    if (local_player_mini_crit_boosted && target_health <= 68 && !carrying_heatmaker)
        return true;

    if (local_player_mini_crit_boosted && target_health <= 54 && carrying_heatmaker)
        return true;

    return false;
}

void doAutoZoom(bool target_found, CachedEntity *target)
{
    bool isIdle = !target_found && hacks::followbot::isIdle();

    // Keep track of our zoom time
    static Timer zoomTime{};

    auto nearest = hacks::NavBot::getNearestPlayerDistance();  

    if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFMinigun))
    {
        if (target_found)
            zoomTime.update();
        if (!(target_found && nearest.second > *zoom_distance))
            current_user_cmd->buttons |= IN_ATTACK2;
        return;
    }
    else if (g_pLocalPlayer->holding_sniper_rifle && !AllowNoScope(target) && (target_found || nearest.second <= *zoom_distance || isIdle))
    {
        if (target_found)
            zoomTime.update();
        if (!g_pLocalPlayer->bZoomed)
            current_user_cmd->buttons |= IN_ATTACK2;
    }
    else if (*auto_unzoom && zoomTime.test_and_set(*zoom_time) && !target_found && g_pLocalPlayer->holding_sniper_rifle && g_pLocalPlayer->bZoomed && nearest.second > *zoom_distance)
        current_user_cmd->buttons |= IN_ATTACK2;
}

// Current Entity
CachedEntity *target_last = 0;
bool aimed_this_tick      = false;
Vector viewangles_this_tick(0.0f);

// If slow aimbot allows autoshoot
bool slow_can_shoot = false;

// This array will store calculated projectile/hitscan predictions
// for current frame, to avoid performing them again
AimbotCalculatedData_s calculated_data_array[2048]{};
// The main "loop" of the aimbot.
bool already_created;
pthread_t pthread_id;
static void CreateMove()
{
    enable   = *normal_enable;
    slow_aim = *normal_slow_aim;
    fov      = *normal_fov;
    aimed_this_tick = false;

    bool aimkey_status = UpdateAimkey();

    if (!enable)
    {
        target_last = nullptr;
        return;
    }
    else if (!LOCAL_E->m_bAlivePlayer() || !g_pLocalPlayer->entity)
    {
        target_last = nullptr;
        return;
    }
    else if (!aimkey_status || !ShouldAim)
    {
        target_last = nullptr;
        return;
    }

    // Proper fix for deathstare
    if (*only_can_shoot && !CanShoot() && !hacks::warp::in_rapidfire)
        return;

    doAutoZoom(false, nullptr);
    CachedEntity *target_entity = target_last = RetrieveBestTarget(aimkey_status);
    bool should_backtrack = hacks::backtrack::backtrackEnabled();
    int get_weapon_mode = g_pLocalPlayer->weapon_mode;
    switch(get_weapon_mode)
    {
        case weapon_hitscan:
        {
            if (should_backtrack)
                UpdateShouldBacktrack();
            if (smallBoxChecker(target_entity))
            {
                    int weapon_case = LOCAL_W->m_iClassID();
                    doAutoZoom(true, target_last);
                    Aim(target_entity);

                    if (!hitscanSpecialCases(target_entity, weapon_case))
                        DoAutoshoot();
                    if (hitscanSpecialCases(target_entity, weapon_case) && (CE_INT(LOCAL_W, netvar.m_iClip1) == 0))
                        DoAutoshoot();
            }
        break;
        }
        case weapon_melee:
        {
            if (should_backtrack)
                UpdateShouldBacktrack();
            if (smallBoxChecker(target_entity))
            {
                if (antiaim::isEnabled())
                {
                    DoAutoshoot();
                    if (g_pLocalPlayer->weapon_melee_damage_tick)
                    {
                        *bSendPackets = false;
                        Aim(target_entity);
                    }
                }
                else
                {
                    Aim(target_entity);
                    DoAutoshoot();
                }
            }

            break;
        }
    }
}

bool hitscanSpecialCases(CachedEntity *target_entity, int weapon_case) 
{
    switch(weapon_case) 
    {
        case CL_CLASS(CTFMinigun): 
        {
            DoAutoshoot(target_entity);
            int tapfire_delay = 0;
            tapfire_delay++;
            if (tapfire_delay == 17 || target_entity->m_flDistance() <= 1250.0f) 
            {
                DoAutoshoot(target_entity);
                return true;
            }
            return true;
        }
        default:
            return false;
    }
}

bool smallBoxChecker(CachedEntity *target_entity)
{
    if (CE_BAD(target_entity) ||!g_IEntityList->GetClientEntity(target_entity->m_IDX))
        return false;
    if (!target_entity->hitboxes.GetHitbox(calculated_data_array[target_entity->m_IDX].hitbox))
        return false;
    #if ENABLE_VISUALS
    if (target_entity->m_Type() == ENTITY_PLAYER)
        hacks::esp::SetEntityColor(target_entity, colors::target);
    #endif
    return true;
}

// Just hold m1 if we were aiming at something before and are in rapidfire
static void CreateMoveWarp()
{
    if (hacks::warp::in_rapidfire && aimed_this_tick)
    {
        current_user_cmd->viewangles     = viewangles_this_tick;
        g_pLocalPlayer->bUseSilentAngles = *silent;
        current_user_cmd->buttons |= IN_ATTACK;
    }
    // Warp should call aimbot normally
    else if (!hacks::warp::in_rapidfire)
        CreateMove();
}

bool ShouldAim()
{
    // Checks should be in order: cheap -> expensive

    // Check for +use && Check if using action slot item
    if (current_user_cmd->buttons & IN_USE || g_pLocalPlayer->using_action_slot_item)
        return false;

    // Carrying A building? && taunting && bonked?
    else if (CE_BYTE(g_pLocalPlayer->entity, netvar.m_bCarryingObject) || HasCondition<TFCond_Taunting>(g_pLocalPlayer->entity) || HasCondition<TFCond_Bonked>(g_pLocalPlayer->entity) || (LOCAL_W->m_iClassID() == CL_CLASS(CTFMinigun) && CE_INT(LOCAL_E, netvar.m_iAmmo + 4) == 0  ))
        return false;
        
    switch (GetWeaponMode())
    {
    case weapon_hitscan:
        break;
    case weapon_melee:
        break;
    // Check if player doesnt have a weapon usable by aimbot
    default:
        return false;
    };

    return true;
}

// Function to find a suitable target
CachedEntity *RetrieveBestTarget(bool aimkey_state)
{
    // If we have a previously chosen target, target lock is on, and the aimkey
    // is allowed, then attemt to keep the previous target

    if (*target_lock && target_last && aimkey_state)
    {

            if (shouldBacktrack(target_last))
            {
                auto good_ticks_tmp = hacks::backtrack::getGoodTicks(target_last);
                if (good_ticks_tmp)
                {
                    auto good_ticks = *good_ticks_tmp;
                    if (backtrackLastTickOnly)
                    {
                        good_ticks.clear();
                        good_ticks.push_back(good_ticks_tmp->back());
                    }
                    for (auto &bt_tick : good_ticks)
                    {
                        if (!validateTickFOV(bt_tick))
                            continue;
                        hacks::backtrack::MoveToTick(bt_tick);
                        if (IsTargetStateGood(target_last))
                            return target_last;
                        // Restore if bad target
                        hacks::backtrack::RestoreEntity(target_last->m_IDX);
                    }
                }
            // Check if previous target is still good
            else if (!shouldbacktrack_cache && IsTargetStateGood(target_last))
            {
                // If it is then return it again
                return target_last;
            }
        }
    }
    // No last_target found, reset the timer.
    hacks::aimbot::last_target_ignore_timer = 0;

    float target_highest_score, scr = 0.0f;
    CachedEntity *ent;
    CachedEntity *target_highest_ent                            = 0;
    target_highest_score                                        = -256;
    std::optional<hacks::backtrack::BacktrackData> bt_tick = std::nullopt;
    for (auto const &ent : entity_cache::valid_ents)
    {
        // Check for null and dormant
        // Check whether the current ent is good enough to target
        bool isTargetGood = false;

        static std::optional<hacks::backtrack::BacktrackData> temp_bt_tick = std::nullopt;
        if (shouldBacktrack(ent))
        {
            auto good_ticks_tmp = backtrack::getGoodTicks(ent);
            if (good_ticks_tmp)
            {
                auto good_ticks = *good_ticks_tmp;
                if (backtrackLastTickOnly)
                {
                    good_ticks.clear();
                    good_ticks.push_back(good_ticks_tmp->back());
                }
                for (auto &bt_tick : good_ticks)
                {
                    if (!validateTickFOV(bt_tick))
                        continue;
                    hacks::backtrack::MoveToTick(bt_tick);
                    if (IsTargetStateGood(ent))
                    {
                        isTargetGood = true;
                        temp_bt_tick = bt_tick;
                        break;
                    }
                    hacks::backtrack::RestoreEntity(ent->m_IDX);
                }
            }
        }
        else
            isTargetGood=IsTargetStateGood(ent);
        
        if (isTargetGood)
        {
            // Check if the current weapon mode is melee or priority mode is 2
            if (GetWeaponMode() == weaponmode::weapon_melee || (int)priority_mode == 2)
            {
                // Distance Priority, uses this if melee is used
                float scr = 0.0f;
                if (GetWeaponMode() == weaponmode::weapon_melee)
                    scr = 4096.0f - cd.aim_position.DistTo(g_pLocalPlayer->v_Eye);

                switch ((int) priority_mode)
                {
                case 0: // Smart Priority
                    scr = GetScoreForEntity_aim(ent);
                    break;
                case 1: // Fov Priority
                    scr = 360.0f - cd.fov;
                    break;
                case 3: // Distance Priority (Furthest Away)
                    scr = cd.aim_position.DistTo(g_pLocalPlayer->v_Eye);
                    break;
                case 4: // Fast
                    return ent;
                default:
                    break;
                }
            }
            // Compare the top score to our current ents score
            if (scr > target_highest_score)
            {
                target_highest_score = scr;
                target_highest_ent   = ent;
                bt_tick              = temp_bt_tick;
            }
        }

        // Restore tick
        if (shouldBacktrack(ent))
            hacks::backtrack::RestoreEntity(ent->m_IDX);
    }
    if (target_highest_ent && bt_tick)
        hacks::backtrack::MoveToTick(*bt_tick);
    return target_highest_ent;
}

// A second check to determine whether a target is good enough to be aimed at
bool IsTargetStateGood(CachedEntity *entity)
{
    PROF_SECTION(PT_aimbot_targetstatecheck);

    const int current_type = entity->m_Type();
    switch(current_type)
    {

    case(ENTITY_PLAYER):
    {
        // Local player check
        if (entity == LOCAL_E)
            return false;
        // Dead
        else if (!entity->m_bAlivePlayer())
            return false;
        // Teammates
        else if (!playerTeamCheck(entity))
            return false;
        else if (!player_tools::shouldTarget(entity))
                return false;
            // Invulnerable players, ex: uber, bonk
        else if (IsPlayerInvulnerable(entity))
                return false;
            
                // Distance
        float targeting_range = EffectiveTargetingRange();

            if (g_pLocalPlayer->weapon_mode != weapon_melee)
            {
                if (entity->m_flDistance() > targeting_range && tickcount > hacks::aimbot::last_target_ignore_timer)
                    return false;
            }
            else
            {
                int hb = BestHitbox(entity);
                if(hb==-1)
                return false;
                Vector newangle = GetAimAtAngles(g_pLocalPlayer->v_Eye, entity->hitboxes.GetHitbox(hb)->center, LOCAL_E);
                trace_t trace;
                Ray_t ray;
                trace::filter_default.SetSelf(RAW_ENT(LOCAL_E));
                ray.Init(g_pLocalPlayer->v_Eye, GetForwardVector(g_pLocalPlayer->v_Eye, newangle, targeting_range, LOCAL_E));
                g_ITrace->TraceRay(ray, MASK_SHOT_HULL, &trace::filter_default, &trace);
                if ((IClientEntity *) trace.m_pEnt != RAW_ENT(entity))
                    return false;
            }

            // Wait for charge
            if (*wait_for_charge && g_pLocalPlayer->holding_sniper_rifle)
            {
                float cdmg  = CE_FLOAT(LOCAL_W, netvar.flChargedDamage) * 3;
                float maxhs = 450.0f;
                if (CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 230 || HasCondition<TFCond_Jarated>(entity))
                {
                    cdmg  = int(CE_FLOAT(LOCAL_W, netvar.flChargedDamage) * 1.35f);
                    maxhs = 203.0f;
                }
                bool maxCharge = cdmg >= maxhs;

                // Darwins damage correction, Darwins protects against 15% of
                // damage
                //                if (HasDarwins(entity))
                //                    cdmg = (cdmg * .85) - 1;
                // Vaccinator damage correction, Vac charge protects against 75%
                // of damage
                if (IsPlayerInvisible(entity))
                    cdmg = (cdmg * .80) - 1;

                else if (HasCondition<TFCond_UberBulletResist>(entity))
                {
                    cdmg = (cdmg * .25) - 1;
                    // Passive bullet resist protects against 10% of damage
                }
                else if (HasCondition<TFCond_SmallBulletResist>(entity))
                {
                    cdmg = (cdmg * .90) - 1;
                }
                float hsdmg = 150.0f;
                if (CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 230)
                    hsdmg = int(50.0f * 1.35f);

                int health = entity->m_iHealth();
                if (!(health <= hsdmg || health <= cdmg || !g_pLocalPlayer->bZoomed || (maxCharge && health > maxhs)))
                {
                    return false;
                }
            }

        AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];
        cd.hitbox = BestHitbox(entity);
         if (*vischeck_hitboxes && !*multipoint)
        {
            if (*vischeck_hitboxes == 1 && playerlist::AccessData(entity).state != playerlist::k_EState::RAGE)
                return true;
            else
            {
                int i = 0;
                trace_t first_tracer;
                if (IsEntityVectorVisible(entity, entity->hitboxes.GetHitbox(cd.hitbox)->center, true, MASK_SHOT_HULL, &first_tracer))
                    return true;
                while (i <= 17) // Prevents returning empty at all costs. Loops through every hitbox
                {
                    if (i == cd.hitbox)
                        i++;
                    trace_t test_trace;
                    std::vector<Vector> centered_hitbox = GetHitpointsVischeck(entity, i);

                    if (IsEntityVectorVisible(entity, centered_hitbox[0], true, MASK_SHOT_HULL, &test_trace))
                    {
                        cd.hitbox = i;
                        return true;
                    }
                    i++;
                }
                return false; // It looped through every hitbox and found nothing. It isn't visible.
            }
        }
        bool vis_check = VischeckPredictedEntity(entity);
        // Vis check + fov check
        if (!vis_check)
            return false;
        else if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
        {
            int sentry = GetSentry();
            if (sentry == -1)
                return false;
            Vector pos = GetBuildingPosition(ENTITY(sentry));
            if (cd.hitbox == -1 || !entity->hitboxes.GetHitbox(cd.hitbox))
                return false;
            if (!IsVectorVisible(pos, entity->hitboxes.GetHitbox(cd.hitbox)->center, false, ENTITY(sentry)))
                return false;
        }
        else if (fov > 0.0f && cd.fov > fov && tickcount > hacks::aimbot::last_target_ignore_timer)
            return false;

        return true;
        break;
    }
    // Check for buildings
    case(ENTITY_BUILDING):
    {
        // Enabled check
        if (!(buildings_other || buildings_sentry) || !entity->m_bEnemy())
            return false;

            if (entity->m_flDistance() > (int) EffectiveTargetingRange() && tickcount > hacks::aimbot::last_target_ignore_timer)
                return false;

        // Building type
        else if (!(buildings_sentry && buildings_other))
        {
            // Check if target is a sentrygun
            if (entity->m_iClassID() == CL_CLASS(CObjectSentrygun))
            {
                if (!buildings_sentry && buildings_other)
                    return false;
            }
        }
         // Grab the prediction var
        AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];

        // Vis and fov checks
        if (!VischeckPredictedEntity(entity))
            return false;
        if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
        {
            int sentry = GetSentry();
            if (sentry == -1)
                return false;
            Vector pos = GetBuildingPosition(ENTITY(sentry));
            if (!IsVectorVisible(pos, GetBuildingPosition(entity), false, ENTITY(sentry)))
                return false;
        }
        if (fov > 0.0f && cd.fov > fov && tickcount > hacks::aimbot::last_target_ignore_timer)
            return false;

        return true;
    }
    // NPCs (Skeletons, Merasmus, etc)
    case(ENTITY_NPC):
    {
        // NPC targeting is disabled
        if (!npcs)
            return false;

        // Cannot shoot this
        else if (entity->m_iTeam() == LOCAL_E->m_iTeam())
            return false;

        // Distance
        float targeting_range = EffectiveTargetingRange();
        if (targeting_range)
        {
            if (entity->m_flDistance() > targeting_range && tickcount > hacks::aimbot::last_target_ignore_timer)
                return false;
        }

        // Grab the prediction var
        AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];
        bool vischeck_real = VischeckPredictedEntity(entity);
        if (vischeck_real)
            return false;
        else if (fov > 0.0f && cd.fov > fov && tickcount > hacks::aimbot::last_target_ignore_timer)
            return false;
        return true;
        break;
    }
    default:
    break;
}
    // Check for stickybombs
    if (entity->m_iClassID() == CL_CLASS(CTFGrenadePipebombProjectile))
    {
        // Enabled
        if (!stickybot)
            return false;

        // Only hitscan weapons can break stickys so check for them.
        else if (!(GetWeaponMode() == weapon_hitscan))
            return false;

        // Distance
        float targeting_range = EffectiveTargetingRange();
            if (entity->m_flDistance() > targeting_range)
                return false;

        // Teammates, Even with friendly fire enabled, stickys can NOT be
        // destroied
        if (!entity->m_bEnemy())
            return false;

        // Check if target is a pipe bomb
        if (CE_INT(entity, netvar.iPipeType) != 1)
            return false;

        // Moving Sticky?
        Vector velocity;
        velocity::EstimateAbsVelocity(RAW_ENT(entity), velocity);
        if (!velocity.IsZero())
            return false;

        // Grab the prediction var
        AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];

    if (!VischeckPredictedEntity(entity))
    return false;

if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
{
    int sentry = GetSentry();
    if (sentry == -1 || !IsVectorVisible(GetBuildingPosition(ENTITY(sentry)), entity->m_vecOrigin(), false))
        return false;
}

if (fov > 0.0f && cd.fov > fov)
    return false;

return true;
    }
    }

// A function to aim at a specific entitiy
void Aim(CachedEntity *entity)
{
    // Special case for miniguns
    if (only_can_shoot/* && LOCAL_W->m_iClassID() == CL_CLASS(CTFMinigun)*/ && !CanShoot() && !hacks::warp::in_rapidfire)
        return;
    // Get angles from eye to target
    Vector angles = GetAimAtAngles(g_pLocalPlayer->v_Eye, PredictEntity(entity, false), LOCAL_E);

    // Slow aim
    if (slow_aim)
        DoSlowAim(angles);

    // Set angles
    current_user_cmd->viewangles = angles;

    if (silent && !slow_aim)
        g_pLocalPlayer->bUseSilentAngles = true;
    // Set tick count to targets (backtrack messes with this)
    if (!shouldBacktrack(entity) && *nolerp && entity->m_IDX <= g_IEngine->GetMaxClients())
        current_user_cmd->tick_count = TIME_TO_TICKS(CE_FLOAT(entity, netvar.m_flSimulationTime));
    aimed_this_tick      = true;
    viewangles_this_tick = angles;

    return;
}

// A function to check whether player can autoshoot
bool begancharge = false;
void DoAutoshoot(CachedEntity *target_entity)
{
    bool attack = true;
    // Enable check
    if (!*autoshoot)
        return;
    // Check if we can shoot, ignore during rapidfire (special case for minigun: we can shoot 24/7 just dont aim, idk why this works)
    if (*only_can_shoot && !CanShoot() && !hacks::warp::in_rapidfire && LOCAL_W->m_iClassID() != CL_CLASS(CTFMinigun))
        return;
    // Rifle check
    if (g_pLocalPlayer->holding_sniper_rifle && *zoomed_only && !CanHeadshot() && !AllowNoScope(target_entity))
        attack = false;
    // Autoshoot breaks with Slow aimbot, so use a workaround to detect when it can
    if (slow_aim && !slow_can_shoot)
        attack = false;

    // Dont autoshoot without anything in clip
    if (CE_INT(g_pLocalPlayer->weapon(), netvar.m_iClip1) == 0)
        attack = false;

    if (attack)
    {
        // TO DO: Sending both reload and attack will activate the hitmans heatmaker ability
        // Don't activate it only on first kill (or somehow activate it before a shot)
        current_user_cmd->buttons |= IN_ATTACK | (*autoreload && CarryingHeatmaker() ? IN_RELOAD : 0);
        if (target_entity)
        {
            auto hitbox = cd.hitbox;
            hitrate::AimbotShot(target_entity->m_IDX, hitbox != head);
        }
        *bSendPackets = true;
    }
    if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
        current_user_cmd->buttons |= IN_ATTACK2;
}

// Grab a vector for a specific entity
Vector PredictEntity(CachedEntity *entity, bool vischeck)
{
    // Pull out predicted data
    AimbotCalculatedData_s& cd = calculated_data_array[entity->m_IDX];
    Vector& result = cd.aim_position;

    if (cd.predict_tick == tickcount && cd.predict_type == vischeck && !shouldBacktrack(entity))
    {
        return result;
    }

    const short int curr_type = entity->m_Type();
    // If using projectiles, predict a vector
    if (curr_type == ENTITY_PLAYER)
    {
        // Allow multipoint logic to run
        if (!*multipoint)
        {
            result = entity->hitboxes.GetHitbox(cd.hitbox)->center;
        }
        else
        {
            std::optional<Vector> best_pos = GetBestHitpoint(entity, cd.hitbox);
            if (best_pos)
            {
                result = *best_pos;
            }
            else
            {
                result = entity->hitboxes.GetHitbox(cd.hitbox)->center;
            }
        }
    }
    else if (curr_type == ENTITY_BUILDING)
    {
        result = GetBuildingPosition(entity);
    }
    else if (curr_type == ENTITY_NPC)
    {
        result = entity->hitboxes.GetHitbox(std::max(0, entity->hitboxes.GetNumHitboxes() / 2 - 1))->center;
    }
    else
    {
        result = entity->m_vecOrigin();
    }

    cd.predict_tick = tickcount;
    cd.predict_type = vischeck;

    cd.fov = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, result);

    // Return the found vector
    return result;
}

int notVisibleHitbox(CachedEntity *target, int preferred)
{
    if (target->hitboxes.VisibilityCheck(preferred))
        return preferred;

    return hitbox_t::spine_1;
}

int autoHitbox(CachedEntity *target)
{
            int preferred=3;
            bool headonly = false;
            int target_health = target->m_iHealth(); // This was used way too many times. Due to how pointers work (defrencing)+the compiler already dealing with tons of AIDS global variables it likely derefrenced it every time it was called.
            int ci    = LOCAL_W->m_iClassID();

            if (CanHeadshot()) // Nothing else zooms in this game you have to be holding a rifle for this to be true.
            {
                float cdmg = CE_FLOAT(LOCAL_W, netvar.flChargedDamage);
                float bdmg = 50;
                // Vaccinator damage correction, protects against 20% of damage
                if (CarryingHeatmaker())
                {
                    bdmg = (bdmg * .80) - 1;
                    cdmg = (cdmg * .80) - 1;
                }
                // Vaccinator damage correction, protects against 75% of damage
                if (HasCondition<TFCond_UberBulletResist>(target))
                {
                    bdmg = (bdmg * .25) - 1;
                    cdmg = (cdmg * .25) - 1;
                }
                // Passive bullet resist protects against 10% of damage
                else if (HasCondition<TFCond_SmallBulletResist>(target))
                {
                    bdmg = (bdmg * .90) - 1;
                    cdmg = (cdmg * .90) - 1;
                }
                // Invis damage correction, Invis spies get protection from 10%
                // of damage
                else if (IsPlayerInvisible(target)) // You can't be invisible and under the effects of the vacc
                {
                    bdmg = (bdmg * .80) - 1;
                    cdmg = (cdmg * .80) - 1;
                }
                // If can headshot and if bodyshot kill from charge damage, or
                // if crit boosted and they have 150 health, or if player isnt
                // zoomed, or if the enemy has less than 40, due to darwins, and
                // only if they have less than 150 health will it try to
                // bodyshot
                if (std::floor(cdmg) >= target_health|| IsPlayerCritBoosted(g_pLocalPlayer->entity) || target_health <= std::floor(bdmg) && target_health <= 150)
                { /* hit body if low hp */
                    preferred = hitbox_t::spine_1;
                    headonly  = false;
                    return preferred;
                }

                return hitbox_t::head;
            }
}

int ClosestHitbox(CachedEntity *target, float &closest_fov)
{
    int closest = -1;
    closest_fov = 256.0f;

    for (int i = 0; i < target->hitboxes.GetNumHitboxes(); ++i)
    {
        float fov = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, target->hitboxes.GetHitbox(i)->center);
        if (fov < closest_fov)
        {
            closest = i;
            closest_fov = fov;
        }
    }

    return closest;
}

// Function to get predicted visual checks
bool VischeckPredictedEntity(CachedEntity *entity)
{
    // Retrieve predicted data
    AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];
    if (cd.vcheck_tick == tickcount && !shouldBacktrack(entity))
        return cd.visible;
    // Update info
    cd.vcheck_tick = tickcount;
    if (entity->m_Type() != ENTITY_PLAYER)
        cd.visible = IsEntityVectorVisible(entity, PredictEntity(entity, true), true);
    else
    {
        trace_t trace;
        cd.visible = IsEntityVectorVisible(entity, PredictEntity(entity, true), false, MASK_SHOT, &trace);
        if (cd.visible && cd.hitbox == head && trace.hitbox != head)
            cd.visible = false;
    }

    return cd.visible;
}

// A helper function to find a user angle that isnt directly on the target
// angle, effectively slowing the aiming process
void DoSlowAim(Vector &input_angle)
{
    auto viewangles   = current_user_cmd->viewangles;
    Vector slow_delta = { 0, 0, 0 };

    // Don't bother if we're already on target
    if (viewangles != input_angle)
    {
        slow_delta = input_angle - viewangles;

        while (slow_delta.y > 180)
            slow_delta.y -= 360;
        while (slow_delta.y < -180)
            slow_delta.y += 360;

        slow_delta /= slow_aim;
        input_angle = viewangles + slow_delta;

        // Clamp as we changed angles
        fClampAngle(input_angle);
    }
    // 0.17 is a good amount in general
    slow_can_shoot = false;
    if (std::abs(slow_delta.y) < 0.18 && std::abs(slow_delta.x) < 0.18)
        slow_can_shoot = true;
}

// A function that determines whether aimkey allows aiming
bool UpdateAimkey()
{
    static bool aimkey_flip = false;
    static bool pressed_last_tick = false;
    bool allow_aimkey = true;
    bool last_allow_aimkey = true;
    // Check if aimkey is used
    if (aimkey && aimkey_mode)
    {
        // Grab whether the aimkey is depressed
        bool key_down = aimkey.isKeyDown();
        switch ((int) aimkey_mode)
        {
        case 1: // Only while key is depressed
            if (!key_down)
                allow_aimkey = false;
            break;
        case 2: // Only while key is not depressed, enable
            if (key_down)
                allow_aimkey = false;
            break;
        case 3: // Aimkey acts like a toggle switch
            if (!pressed_last_tick && key_down)
                aimkey_flip = !aimkey_flip;
            if (!aimkey_flip)
                allow_aimkey = false;
            break;
        default:
            break;
        }
        last_allow_aimkey = allow_aimkey;
        pressed_last_tick = key_down;
    }
    // Return whether the aimkey allows aiming
    return allow_aimkey;
}

// Used mostly by navbot to not accidentally look at path when aiming
bool isAiming()
{
    return aimed_this_tick;
}

// A function used by gui elements to determine the current target
CachedEntity *CurrentTarget()
{
    return target_last;
}

// Used for when you join and leave maps to reset aimbot vars
void Reset()
{
    target_last     = nullptr;
}

void rvarCallback(settings::VariableBase<float> &, float after)
{
    force_backtrack_aimbot = after >= 200.0f;
}

static InitRoutine EC(
    []()
    {
        hacks::backtrack::latency.installChangeCallback(rvarCallback);
        EC::Register(EC::LevelInit, Reset, "INIT_Aimbot", EC::average);
        EC::Register(EC::LevelShutdown, Reset, "RESET_Aimbot", EC::average);
        EC::Register(EC::CreateMove, CreateMove, "CM_Aimbot", EC::late);
        EC::Register(EC::CreateMoveWarp, CreateMoveWarp, "CMW_Aimbot", EC::late);
    });

} // namespace hacks::aimbot