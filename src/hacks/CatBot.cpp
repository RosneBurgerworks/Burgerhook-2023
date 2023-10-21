#include <settings/Bool.hpp>
#include "CatBot.hpp"
#include "common.hpp"
#include "hack.hpp"
#include "PlayerTools.hpp"
#include "MiscTemporary.hpp"

static settings::Boolean abandon_if{ "cat-bot.abandon-if.enable", "false" };

static settings::Int abandon_if_bots_gte{ "cat-bot.abandon-if.bots-gte", "0" };
static settings::Int abandon_if_ipc_bots_gte{ "cat-bot.abandon-if.ipc-bots-gte", "0" };
static settings::Int abandon_if_humans_lte{ "cat-bot.abandon-if.humans-lte", "0" };
static settings::Int requeue_if_humans_lte{ "cat-bot.requeue-if.humans-lte", "0" };
static settings::Int abandon_if_players_lte{ "cat-bot.abandon-if.players-lte", "0" };
static settings::Int abandon_if_team_lte{ "cat-bot.abandon-if.team-lte", "0" };
static settings::Int abandon_if_timer{ "cat-bot.abandon-if.hour.timer", "1" };
#if ENABLE_TEXTMODE
  static settings::Boolean abandon_if_hour{"cat-bot.abandon-if.hour", "true"};
#elif ENABLE_IPC
  static settings::Boolean abandon_if_hour{"cat-bot.abandon-if.hour", "false"};
#endif

static settings::Boolean micspam{ "cat-bot.micspam.enable", "false" };
static settings::Int micspam_on{ "cat-bot.micspam.interval-on", "1" };
static settings::Int micspam_off{ "cat-bot.micspam.interval-off", "0" };

static settings::Boolean autovote_map{ "cat-bot.autovote-map", "true" };

namespace hacks::catbot
{
settings::Boolean catbotmode{ "cat-bot.enable", "true" };
settings::Boolean anti_motd{ "cat-bot.anti-motd", "false" };

static Timer timer_catbot_list{};
static Timer timer_abandon{};
Timer level_init_timer{};
Timer micspam_on_timer{}, micspam_off_timer{};

static std::string health = "Health: 0/0";
static std::string ammo   = "Ammo: 0/0";
static int max_ammo;
static CachedEntity *local_w;
// TODO: remove more stuffs
static void cm()
{
    if (!*catbotmode)
        return;

    if (CE_GOOD(LOCAL_E))
    {
        if (LOCAL_W != local_w)
        {
            local_w  = LOCAL_W;
            max_ammo = 0;
        }
        float max_hp  = g_pPlayerResource->GetMaxHealth(LOCAL_E);
        float curr_hp = CE_INT(LOCAL_E, netvar.iHealth);
        int ammo0     = CE_INT(LOCAL_E, netvar.m_iClip2);
        int ammo2     = CE_INT(LOCAL_E, netvar.m_iClip1);
        if (ammo0 + ammo2 > max_ammo)
            max_ammo = ammo0 + ammo2;
        health = format("Health: ", curr_hp, "/", max_hp);
        ammo   = format("Ammo: ", ammo0 + ammo2, "/", max_ammo);
    }
    
    if (g_Settings.bInvalid)
        return;

    if (CE_BAD(LOCAL_E) || CE_BAD(LOCAL_W))
        return;
}

int count_ipc{ 0 };
static std::vector<unsigned> ipc_list{ 0 };
static std::vector<unsigned> ipc_blacklist{};

static bool waiting_for_quit_bool{ false };
static Timer waiting_for_quit_timer{};

#if ENABLE_IPC
void update_ipc_data(ipc::user_data_s &data)
{
    data.ingame.bot_count = count_ipc;
}

static Timer abandon_unix_timestamps{};

static void Paint() 
{
    if (abandon_if && abandon_if_hour && ipc::peer) 
    {
        if (abandon_unix_timestamps.test_and_set(30000)) 
        {
            int diff = time(nullptr) - ipc::peer->memory->peer_user_data[ipc::peer->client_id].ts_connected;

            if (diff >= *abandon_if_timer * 60^2) 
            {
                logging::Info("We have been in a match for too long! abandoning");

                tfmm::abandon();
                g_IEngine->ClientCmd_Unrestricted("killserver;disconnect");
            }
        }
    }
}

#endif

void update()
{
    if (!*catbotmode)
        return;

    if (g_Settings.bInvalid)
        return;

    if (CE_BAD(LOCAL_E))
        return;

#if ENABLE_TEXTMODE
    static Timer unstuck{};
    if (LOCAL_E->m_bAlivePlayer())
        unstuck.update();
    if (unstuck.test_and_set(20000))
        hack::command_stack().push("menuclosed");
#endif

    if (micspam)
    {
        if (micspam_on && micspam_on_timer.test_and_set(*micspam_on * 1000))
            g_IEngine->ClientCmd_Unrestricted("+voicerecord");
        if (micspam_off && micspam_off_timer.test_and_set(*micspam_off * 1000))
            g_IEngine->ClientCmd_Unrestricted("-voicerecord");
    }

    if (abandon_if && timer_abandon.test_and_set(2000) && level_init_timer.check(13000))
    {
        count_ipc = 0;
        ipc_list.clear();
        int count_total = 0;
        int count_team  = 0;
        int count_bot   = 0;

        auto lobby = CTFLobbyShared::GetLobby();
        if (!lobby)
            return;

        CTFLobbyPlayer *player;
        CTFLobbyPlayer *local_player = lobby->GetPlayer(lobby->GetMemberIndexBySteamID(g_ISteamUser->GetSteamID()));
        if (!local_player)
            return;

        int i, members, local_team;
        local_team = local_player->GetTeam();

        members = lobby->GetNumMembers();
        for (i = 0; i < members; ++i)
        {
            player = lobby->GetPlayer(i);
            if (!player)
                continue;
            int team  = player->GetTeam();
            uint32 id = player->GetID().GetAccountID();

            ++count_total;
            if (team == local_team)
                count_team++;

            auto &pl   = playerlist::AccessData(id);
            auto state = pl.state;

            if (state == playerlist::k_EState::PARTY)
                count_bot++;
            if (state == playerlist::k_EState::IPC)
            {
                ipc_list.push_back(id);
                count_ipc++;
                count_bot++;
            }
        }

        if (abandon_if_ipc_bots_gte)
        {
            if (count_ipc >= int(abandon_if_ipc_bots_gte))
            {
                // Store local IPC Id and assign to the quit_id variable for later comparisions
                unsigned local_ipcid = ipc::peer->client_id;
                unsigned quit_id     = local_ipcid;

                // Iterate all the players marked as bot
                for (auto &id : ipc_list)
                {
                    // We already know we shouldn't quit, so just break out of the loop
                    if (quit_id < local_ipcid)
                        break;

                    // Reduce code size
                    auto &peer_mem = ipc::peer->memory;

                    // Iterate all ipc peers
                    for (unsigned i = 0; i < cat_ipc::max_peers; ++i)
                    {
                        // If that ipc peer is alive and in has the steamid of that player
                        if (!peer_mem->peer_data[i].free && peer_mem->peer_user_data[i].friendid == id)
                        {
                            // Check against blacklist
                            if (std::find(ipc_blacklist.begin(), ipc_blacklist.end(), i) != ipc_blacklist.end())
                                continue;

                            // Found someone with a lower ipc id
                            if (i < local_ipcid)
                            {
                                quit_id = i;
                                break;
                            }
                        }
                    }
                }
                // Only quit if you are the player with the lowest ipc id
                if (quit_id == local_ipcid)
                {
                    // Clear blacklist related stuff
                    waiting_for_quit_bool = false;
                    ipc_blacklist.clear();

                    logging::Info("Abandoning because there are %d local players "
                                  "in game, and abandon_if_ipc_bots_gte is %d.",
                                  count_ipc, int(abandon_if_ipc_bots_gte));
                    tfmm::abandon();
                    return;
                }
                else
                {
                    if (!waiting_for_quit_bool)
                    {
                        // Waiting for that ipc id to quit, we use this timer in order to blacklist
                        // ipc peers which refuse to quit for some reason
                        waiting_for_quit_bool = true;
                        waiting_for_quit_timer.update();
                    }
                    else
                    {
                        // IPC peer isn't leaving, blacklist for now
                        if (waiting_for_quit_timer.test_and_set(10000))
                        {
                            ipc_blacklist.push_back(quit_id);
                            waiting_for_quit_bool = false;
                        }
                    }
                }
            }
            else
            {
                // Reset Bool because no reason to quit
                waiting_for_quit_bool = false;
                ipc_blacklist.clear();
            }
        }
        if (abandon_if_humans_lte)
        {
            if (count_total - count_ipc <= int(abandon_if_humans_lte))
            {
                logging::Info("Abandoning because there are %d non-bots in "
                              "game, and abandon_if_humans_lte is %d.",
                              count_total - count_ipc, int(abandon_if_humans_lte));
                tfmm::abandon();
                return;
            }
        }
        /* Check this so we don't spam logs */
        re::CTFGCClientSystem *gc = re::CTFGCClientSystem::GTFGCClientSystem();
        re::CTFPartyClient *pc    = re::CTFPartyClient::GTFPartyClient();
        if (requeue_if_humans_lte && gc && gc->BConnectedToMatchServer(true) && gc->BHaveLiveMatch())
        {
            if (pc && !(pc->BInQueueForMatchGroup(tfmm::getQueue()) || pc->BInQueueForStandby()))
            {
                if (count_total - count_bot <= int(requeue_if_humans_lte))
                {
                    tfmm::startQueue();
                    logging::Info("Requeuing because there are %d non-bots in "
                                    "game, and requeue_if_humans_lte is %d.",
                                    count_total - count_bot, int(requeue_if_humans_lte));
                    return;
                }
            }
        } 
        if (abandon_if_players_lte)
        {
            if (count_total <= int(abandon_if_players_lte))
            {
                logging::Info("Abandoning because there are %d total players "
                                "in game, and abandon_if_players_lte is %d.",
                                count_total, int(abandon_if_players_lte));
                tfmm::abandon();
                return;
            }
        }
        if (abandon_if_bots_gte)
        {
            if (count_bot >= int(abandon_if_bots_gte))
            {
                logging::Info("Abandoning because there are %d total bots "
                                "in game, and abandon_if_bots_gte is %d.",
                                count_total, int(abandon_if_players_lte));
                tfmm::abandon();
                return;
            }
        }
        if (abandon_if_team_lte)
        {
            if (count_team <= int(abandon_if_team_lte))
            {
                logging::Info("Abandoning because there are %d total teammates "
                                "in game, and abandon_if_team_lte is %d.",
                                count_team, int(abandon_if_team_lte));
                    tfmm::abandon();
                return;
            }
        }
    }
}

void level_init()
{
    level_init_timer.update();
}

#if ENABLE_VISUALS
static void draw()
{
    if (!catbotmode || !anti_motd)
        return;
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer())
        return;
}
#endif

class MapVoteListener : public IGameEventListener2
{
    void FireGameEvent(IGameEvent *) override
    {
        if (*catbotmode && *autovote_map)
            g_IEngine->ServerCmd("next_map_vote 0");
    }
};

MapVoteListener &listener2()
{
    static MapVoteListener object{};
    return object;
}

static InitRoutine runinit([]() {
    EC::Register(EC::CreateMove, cm, "cm_catbot", EC::average);
    EC::Register(EC::CreateMove, update, "cm2_catbot", EC::average);
    EC::Register(EC::LevelInit, level_init, "levelinit_catbot", EC::average);
#if ENABLE_VISUALS
    EC::Register(EC::Draw, draw, "draw_catbot", EC::average);
#endif
    g_IEventManager2->AddListener(&listener2(), "vote_maps_changed", false);
});

} // namespace hacks::shared::catbot