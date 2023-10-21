#include "common.hpp"
#include <unordered_map>
#include <playerlist.hpp>
#include "PlayerTools.hpp"
#include "entitycache.hpp"
#include "settings/Bool.hpp"
#include "MiscTemporary.hpp"

namespace player_tools
{

static settings::Boolean taunting{ "player-tools.ignore.taunting", "false" };

bool shouldTargetSteamId(unsigned id)
{
    auto &pl = playerlist::AccessData(id);
    if (playerlist::IsFriendly(pl.state))
        return false;
    return true;
}

bool shouldTarget(CachedEntity *entity)
{
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        if (taunting && HasCondition<TFCond_Taunting>(entity) && CE_INT(entity, netvar.m_iTauntIndex) == 3)
            return false;
        if (HasCondition<TFCond_HalloweenGhostMode>(entity))
            return false;
        // Don't shoot players in truce
        if (isTruce())
            return false;
        return shouldTargetSteamId(entity->player_info.friendsID);
    }
    else if (entity->m_Type() == ENTITY_BUILDING)
        // Don't shoot buildings in truce
        if (isTruce())
            return false;

    return true;
}
bool shouldAlwaysRenderEspSteamId(unsigned id)
{
    return (id != 0 && playerlist::AccessData(id).state != playerlist::k_EState::DEFAULT);
}

bool shouldAlwaysRenderEsp(CachedEntity *entity)
{
    return (entity->m_Type() == ENTITY_PLAYER && shouldAlwaysRenderEspSteamId(entity->player_info.friendsID));
}

#if ENABLE_VISUALS
std::optional<colors::rgba_t> forceEspColorSteamId(unsigned id)
{
    if (id == 0)
        return std::nullopt;

    auto pl = playerlist::Color(id);
    if (pl != colors::empty)
        return std::optional<colors::rgba_t>{ pl };

    return std::nullopt;
}
std::optional<colors::rgba_t> forceEspColor(CachedEntity *entity)
{
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        return forceEspColorSteamId(entity->player_info.friendsID);
    }

    return std::nullopt;
}
#endif

} // namespace player_tools
