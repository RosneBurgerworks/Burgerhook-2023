#include "common.hpp"
#include <boost/algorithm/string.hpp>
#include <settings/Bool.hpp>
#include "votelogger.hpp"
#include "Votekicks.hpp"
#include "PlayerTools.hpp"

static settings::Int vote_wait_min{ "votelogger.autovote.wait.min", "1000" };
static settings::Int vote_wait_max{ "votelogger.autovote.wait.max", "6000" };
static settings::Boolean vote_wait{ "votelogger.autovote.wait", "false" };
static settings::Boolean vote_kicky{ "votelogger.autovote.yes", "false" };
static settings::Boolean vote_kickn{ "votelogger.autovote.no", "false" };
static settings::Boolean vote_rage_vote{ "votelogger.autovote.no.rage", "false" };
static settings::Boolean chat{ "votelogger.chat", "false" };
static settings::Boolean chat_partysay{ "votelogger.chat.partysay", "false" };
static settings::Boolean chat_partysay_result{ "votelogger.chat.partysay.result", "false" };
static settings::Boolean chat_casts{ "votelogger.chat.casts", "false" };
static settings::Boolean abandon_on_local_vote{ "votelogger.leave-after-local-vote", "false" };
static settings::Boolean requeue_on_kick{ "votelogger.requeue-on-kick", "false" };

namespace votelogger
{

static bool was_local_player{ false };
static bool was_local_player_caller{ false };
static Timer local_kick_timer{};
static int kicked_player;
static int F1count = 0;
static int F2count = 0;

void Reset()
{
    was_local_player        = false;
    was_local_player_caller = false;
    F1count                 = 0;
    F2count                 = 0;
}

static void vote_rage_back()
{
    static Timer attempt_vote_time;
    char cmd[40];
    player_info_s info;
    std::vector<int> targets;

    if (!g_IEngine->IsInGame() || !attempt_vote_time.test_and_set(1000))
        return;

    for (auto const &ent : entity_cache::player_cache)
    {
        // TO DO: m_bEnemy check only when you can't vote off players from the opposite team
        if (ent == LOCAL_E || ent->m_bEnemy())
            continue;

        if (!GetPlayerInfo(ent->m_IDX, &info))
            continue;

        auto &pl = playerlist::AccessData(info.friendsID);
        if (pl.state == playerlist::k_EState::RAGE)
            targets.emplace_back(info.userID);
    }
    if (targets.empty())
        return;

    std::snprintf(cmd, sizeof(cmd), "callvote kick \"%d scamming\"", targets[UniformRandomInt(0, targets.size() - 1)]);
    g_IEngine->ClientCmd_Unrestricted(cmd);
}
// Call Vote RNG
struct CVRNG
{
    std::string content;
    unsigned time;
    Timer timer{};
};
static CVRNG vote_command;
void dispatchUserMessage(bf_read &buffer, int type)
{
    /* info is the person getting kicked,*/
    /* info2 is the person calling the kick.*/
    player_info_s info{}, info2{};
    int caller, team, vote_id;
    char reason[64], name[64], formated_string[256];
    switch (type)
    {
    case 45:
        // Vote Failed
        break;
    case 46:
    {
        Reset();
        /* Team where vote occured */
        team = buffer.ReadByte();
        /* Each team can have its own vote */
        vote_id = buffer.ReadLong();
        /* Who called the kick ? */
        caller = buffer.ReadByte();
        /* Reason for vote */
        buffer.ReadString(reason, 64, false, nullptr);
        /* Name of kicked player */
        buffer.ReadString(name, 64, false, nullptr);
        auto target = (unsigned char) buffer.ReadByte();
        buffer.Seek(0);
        target >>= 1;

        if (!GetPlayerInfo(target, &info) || !GetPlayerInfo(caller, &info2))
            break;

        kicked_player = target;
        logging::Info("Vote called to kick %s [U:1:%u] for %s by %s [U:1:%u]", info.name, info.friendsID, reason, info2.name, info2.friendsID);
        
        std::string reason_short;
        std::string reason_s = std::string(reason);
        if (reason_s.find("#TF_vote_kick_player_cheating") != std::string::npos)
            reason_short = "Cheating";
        else if (reason_s.find("#TF_vote_kick_player_scamming") != std::string::npos)
            reason_short = "Scamming";
        else if (reason_s.find("#TF_vote_kick_player_idle") != std::string::npos)
            reason_short = "Idle";
        else if (reason_s.find("#TF_vote_kick_player_other") != std::string::npos)
            reason_short = "Other";
        else
            reason_short = reason_s;
        
        if (info.friendsID == g_ISteamUser->GetSteamID().GetAccountID())
            was_local_player = true;
            local_kick_timer.update();

        if (info2.friendsID == g_ISteamUser->GetSteamID().GetAccountID())
            was_local_player_caller = true;

        if (*vote_kickn || *vote_kicky)
        {
            using namespace playerlist;

            auto &pl             = AccessData(info.friendsID);
            auto &pl_caller      = AccessData(info2.friendsID);
            bool friendly_kicked = !player_tools::shouldTargetSteamId(info.friendsID);
            bool friendly_caller = !player_tools::shouldTargetSteamId(info2.friendsID);

            /* Determine string */
            std::string state = "DEFAULT";
            switch (pl.state)
            {
            case k_EState::PRIVATE:
                state = "PRIVATE";
                break;
            case k_EState::IPC:
                state = "IPC";
                break;
            case k_EState::PARTY:
                state = "PARTY";
                break;
            case k_EState::FRIEND:
                state = "FRIEND";
                break;
            case k_EState::RAGE:
                state = "RAGE";
                break;
            }

            if (*vote_kickn && friendly_kicked)
            {
                if (*vote_wait)
                    vote_command = { format_cstr("vote %d option2", vote_id).get(), *vote_wait_min + (rand() % *vote_wait_max) };
                else
                    vote_command = { format_cstr("vote %d option2", vote_id).get() };
                vote_command.timer.update();
                logging::Info("Voting F2 because %s [U:1:%u] is %s playerlist state", info.name, info.friendsID, state);
                if (*vote_rage_vote && !friendly_caller)
                    pl_caller.state = k_EState::RAGE;
                    logging::Info("Voting F2 because %s [U:1:%u] is %s playerlist state. A Counter-kick will be called on %s [U:1:%u] when we can vote.", info.name, info.friendsID, state, info2.name, info2.friendsID);
            }
            else if (*vote_kicky && !friendly_kicked)
            {
                 if (*vote_wait)
                    vote_command = { format_cstr("vote %d option1", vote_id).get(), *vote_wait_min + (rand() % *vote_wait_max) };
                else
                    vote_command = { format_cstr("vote %d option1", vote_id).get() };
                vote_command.timer.update();
                logging::Info("Voting F1 because %s [U:1:%u] is %s", info.name, info.friendsID);
            }
        }
        if (*chat_partysay)
        {
            std::snprintf(formated_string, sizeof(formated_string), "Kick called: %s => %s (%s)", info2.name, info.name, reason_short.c_str());
            if (chat_partysay)
                re::CTFPartyClient::GTFPartyClient()->SendPartyChat(formated_string);
        }
        if (was_local_player && requeue_on_kick)
            tfmm::startQueue();
#if ENABLE_VISUALS
        if (chat)
            PrintChat("Votekick called: \x07%06X%s\x01 => \x07%06X%s\x01 (%s)", colors::chat::team(g_pPlayerResource->GetTeam(caller)), info2.name, colors::chat::team(g_pPlayerResource->GetTeam(target)), info.name, reason_short.c_str());
#endif
        break;
    }
    case 47:
    {
        logging::Info("Vote passed on %s [U:1:%u]", info.name, info.friendsID);
        /*if (*chat_partysay_result)
        {
            std::snprintf(formated_string, sizeof(formated_string), "Vote passed on %s [U:1:%u] with %i F1's and %i F2's", info.name, info.friendsID, F1count + 1, F2count + 1);
            re::CTFPartyClient::GTFPartyClient()->SendPartyChat(formated_string);
        }*/
        if (was_local_player_caller)
        {
            if (info.friendsID)
                hacks::votekicks::previously_kicked.emplace(info.friendsID);
            if (abandon_on_local_vote)
                tfmm::abandon();
        }
        Reset();
        break;
    }
    case 48:
        logging::Info("Vote failed on %s [U:1:%u]", info.name, info.friendsID);
        /*if (*chat_partysay_result)
        {
            std::snprintf(formated_string, sizeof(formated_string), "Vote failed on %s [U:1:%u] with %i F1's and %i F2's", info.name, info.friendsID, F1count + 1, F2count + 1);
            re::CTFPartyClient::GTFPartyClient()->SendPartyChat(formated_string);
        }*/
        if (was_local_player_caller && abandon_on_local_vote)
            tfmm::abandon();
        if (was_local_player && requeue_on_kick)
            tfmm::leaveQueue();
        Reset();
        break;
    default:
        break;
    }
}
static bool found_message = false;
void onShutdown(std::string message)
{
    if (message.find("Generic_Kicked") == message.npos)
    {
        found_message = false;
        return;
    }
    if (local_kick_timer.check(60000) || !was_local_player)
    {
        found_message = false;
        return;
    }
    else
        found_message = false;
}

static void setup_paint_abandon()
{
    EC::Register(
        EC::Paint,
        []()
        {
            if (vote_command.content != "" && vote_command.timer.test_and_set(vote_command.time))
            {
                g_IEngine->ClientCmd_Unrestricted(vote_command.content.c_str());
                vote_command.content = "";
            }
            if (!found_message)
                return;
            if (local_kick_timer.check(60000) || !local_kick_timer.test_and_set(10000) || !was_local_player)
                return;
        },
        "vote_abandon_restart");
}

static void setup_vote_rage()
{
    EC::Register(EC::CreateMove, vote_rage_back, "vote_rage_back");
}

static void reset_vote_rage()
{
    EC::Unregister(EC::CreateMove, "vote_rage_back");
}

class VoteEventListener : public IGameEventListener
{
public:
    void FireGameEvent(KeyValues *event) override
    {
        if (!*chat_casts && !*chat_partysay && !chat /* && !*chat_partysay_result*/)
            return;
        const char *name = event->GetName();
        if (!strcmp(name, "vote_cast"))
        {
            bool vote_option = event->GetInt("vote_option");
            if (vote_option)
                F2count++;
            if (!vote_option)
                F1count++;
            int eid = event->GetInt("entityid");

            player_info_s info{};
            if (!GetPlayerInfo(eid, &info))
                return;
            if (chat_partysay)
            {
                char formated_string[256];
                std::snprintf(formated_string, sizeof(formated_string), "%s [U:1:%u] voted %s", info.name, info.friendsID, vote_option ? "F2" : "F1");

                re::CTFPartyClient::GTFPartyClient()->SendPartyChat(formated_string);
            }
        }
    }
};

static VoteEventListener listener{};
static InitRoutine init(
    []()
    {
        if (*vote_rage_vote)
            setup_vote_rage();
        setup_paint_abandon();

        vote_rage_vote.installChangeCallback(
            [](settings::VariableBase<bool> &var, bool new_val)
            {
                if (new_val)
                    setup_vote_rage();
                else
                    reset_vote_rage();
            });
        g_IGameEventManager->AddListener(&listener, false);
        EC::Register(
            EC::Shutdown, []() { g_IGameEventManager->RemoveListener(&listener); }, "event_shutdown_vote");
    });
} // namespace votelogger
