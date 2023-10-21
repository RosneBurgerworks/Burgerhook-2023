#include <settings/Bool.hpp>
#include "common.hpp"
#include "hooks.hpp"

namespace hacks::killstreak
{

static settings::Boolean enable{ "killstreak.enable", "false" };

int killstreak = 0;
std::unordered_map<int, int> ks_map;

void reset() 
{
    killstreak = 0;
    ks_map.clear();
}

int current_streak() 
{
    return killstreak;
}

int current_weapon_streak() 
{
    return ks_map[LOCAL_W->m_IDX];
}

void apply_killstreaks() 
{
    if (!enable) return;
    
    IClientEntity* ent = g_IEntityList->GetClientEntity(g_IEngine->GetLocalPlayer());
    IClientEntity* resource = g_IEntityList->GetClientEntity(g_pPlayerResource->entity);
    if (!ent || !resource || resource->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE) return;

    int* streaks_resource = (int*)((unsigned)resource + netvar.m_nStreaks_Resource + 4 * g_IEngine->GetLocalPlayer());
    if (*streaks_resource != current_streak()) {
        for (int i = 0; i < 4; ++i) streaks_resource[i] = current_streak();
    }

    int* streaks_player = (int*)ent + netvar.m_nStreaks_Player;
    for (int i = 0; i < 4; ++i) streaks_player[i] = current_streak();
}

void on_kill(IGameEvent* event) {
    const int killer_id = GetPlayerForUserID(event->GetInt("attacker"));
    const int victim_id = GetPlayerForUserID(event->GetInt("userid"));

    if (victim_id == g_IEngine->GetLocalPlayer()) {
        reset();
        return;
    }

    if (killer_id != g_IEngine->GetLocalPlayer() || killer_id == victim_id || CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer()) {
        return;
    }

    killstreak++;
    ks_map[LOCAL_W->m_IDX]++;
    event->SetInt("kill_streak_total", current_streak());
    event->SetInt("kill_streak_wep", current_weapon_streak());

    apply_killstreaks();
}

void on_spawn(IGameEvent* event) {
    int userid = GetPlayerForUserID(event->GetInt("userid"));

    if (userid == g_IEngine->GetLocalPlayer()) 
    {
        reset();
    }
    apply_killstreaks();
}

void fire_event(IGameEvent* event) {
    if (!enable) return;

    if (0 == strcmp(event->GetName(), "player_death")) 
    {
        on_kill(event);
    } 
    else if (0 == strcmp(event->GetName(), "player_spawn")) 
    {
        on_spawn(event);
    }
}

hooks::VMTHook hook;

// Unused, for now.
// TODO: Possibly remove some day?
typedef bool (*FireEvent_t)(IGameEventManager2 *, IGameEvent *, bool);
bool FireEvent_hook(IGameEventManager2 *manager, IGameEvent *event, bool bDontBroadcast)
{
    static FireEvent_t original = (FireEvent_t) hook.GetMethod(offsets::FireEvent());
    fire_event(event);
    return original(manager, event, bDontBroadcast);
}

typedef bool (*FireEventClientSide_t)(IGameEventManager2 *, IGameEvent *);
bool FireEventClientSide(IGameEventManager2 *manager, IGameEvent *event)
{
    static FireEventClientSide_t original = (FireEventClientSide_t) hook.GetMethod(offsets::FireEventClientSide());
    fire_event(event);
    return original(manager, event);
}

void init()
{
}

static InitRoutine EC(
    []()
    {
        EC::Register(EC::Paint, apply_killstreaks, "killstreak", EC::average);
    });

} // namespace hacks::killstreak