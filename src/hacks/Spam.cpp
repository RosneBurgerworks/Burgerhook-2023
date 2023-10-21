#include <hacks/Spam.hpp>
#include "common.hpp"
#include "MiscTemporary.hpp"
#include "PlayerTools.hpp"

namespace hacks::spam
{

static settings::Int spam_source{ "spam.source", "0" };
static settings::Boolean random_order{ "spam.random", "false" };
static settings::String filename{ "spam.filename", "spam.txt" };
static settings::Int spam_delay{ "spam.delay", "4500" };
static settings::Int voicecommand_spam{ "spam.voicecommand", "0" };
static settings::Boolean team_only{ "spam.teamchat", "false" };
static settings::Boolean query_static{ "spam.query-static", "true" };

static size_t last_index;

std::chrono::time_point<std::chrono::system_clock> last_spam_point{};

static size_t current_index{ 0 };
static TextFile file{};

const std::string teams[] = { "RED", "BLU" };

enum class QueryFlags
{
    ZERO        = 0,
    TEAMMATES   = (1 << 0),
    ENEMIES     = (1 << 1),
    STATIC      = (1 << 2),
    ALIVE       = (1 << 3),
    DEAD        = (1 << 4),
    LOCALPLAYER = (1 << 5)
};

struct Query
{
    int flags{ 0 };
};

static int current_static_index{ 0 };
static Query static_query{};

bool PlayerPassesQuery(Query query, int idx)
{
    player_info_s info;

    if (idx == g_IEngine->GetLocalPlayer())
    {
        if (!(query.flags & static_cast<int>(QueryFlags::LOCALPLAYER)))
            return false;
    }

    if (!GetPlayerInfo(idx, &info))
        return false;

    /* friends and local player shouldnt be set as a target */
    if (!player_tools::shouldTargetSteamId(info.friendsID) && !(query.flags & static_cast<int>(QueryFlags::LOCALPLAYER)))
        return false;

    CachedEntity *player = ENTITY(idx);
    if (!RAW_ENT(player))
        return false;

    int teammate = !player->m_bEnemy();
    bool alive   = !CE_BYTE(player, netvar.iLifeState);

    if (query.flags & (static_cast<int>(QueryFlags::TEAMMATES) | static_cast<int>(QueryFlags::ENEMIES)))
    {
        if (!teammate && !(query.flags & static_cast<int>(QueryFlags::ENEMIES)))
            return false;
        if (teammate && !(query.flags & static_cast<int>(QueryFlags::TEAMMATES)))
            return false;
    }

    if (query.flags & (static_cast<int>(QueryFlags::ALIVE) | static_cast<int>(QueryFlags::DEAD)))
    {
        if (!alive && !(query.flags & static_cast<int>(QueryFlags::DEAD)))
            return false;
        if (alive && !(query.flags & static_cast<int>(QueryFlags::ALIVE)))
            return false;
    }

    return true;
}

Query QueryFromSubstring(const std::string &string)
{
    Query result{};
    bool read = true;
    for (auto it = string.begin(); read && *it; it++)
    {
        if (*it == '%')
            read = false;
        if (read)
        {
            switch (*it)
            {
            case 's':
                if (query_static)
                result.flags |= static_cast<int>(QueryFlags::STATIC);
                break;
            case 'a':
                result.flags |= static_cast<int>(QueryFlags::ALIVE);
                break;
            case 'd':
                result.flags |= static_cast<int>(QueryFlags::DEAD);
                break;
            case 't':
                result.flags |= static_cast<int>(QueryFlags::TEAMMATES);
                break;
            case 'e':
                result.flags |= static_cast<int>(QueryFlags::ENEMIES);
                break;
            }
        }
    }

    return result;
}

int QueryPlayer(Query query)
{
    if (query.flags & static_cast<int>(QueryFlags::STATIC))
    {
        if (current_static_index && (query.flags & static_query.flags) == static_query.flags)
        {
            if (PlayerPassesQuery(query, current_static_index))
            {
                return current_static_index;
            }
        }
    }
    std::vector<int> candidates{};
    int index_result = 0;
    for (auto const &ent: entity_cache::player_cache)
    {
        int i = ent->m_IDX;
        if (PlayerPassesQuery(query, i))
        {
            candidates.push_back(i);
        }
    }
    if (candidates.size())
    {
        index_result = candidates.at(rand() % candidates.size());
    }
    if (query.flags & static_cast<int>(QueryFlags::STATIC))
    {
        current_static_index     = index_result;
        static_query.flags       = query.flags;
    }

    return index_result;
}

bool SubstituteQueries(std::string &input)
{
    size_t index = input.find("%query:");
    while (index != std::string::npos)
    {
        std::string sub = input.substr(index + 7);
        size_t closing  = sub.find("%");
        Query q         = QueryFromSubstring(sub);
        int p           = QueryPlayer(q);
        if (!p)
            return false;
        player_info_s info;
        if (!GetPlayerInfo(p, &info))
            return false;
        std::string name = std::string(info.name);
        input.replace(index, 8 + closing, name);
        index = input.find("%query:", index + name.size());
    }

    return true;
}

bool FormatSpamMessage(std::string &message)
{
    ReplaceSpecials(message);
    bool team       = g_pLocalPlayer->team - 2;
    bool enemy_team = !team;
    {
        ReplaceString(message, "%myteam%", teams[team]);
        ReplaceString(message, "%enemyteam%", teams[enemy_team]);
    }
    return SubstituteQueries(message);
}

static void CreateMove()
{
    if (voicecommand_spam)
    {
        static Timer last_voice_spam;
        if (last_voice_spam.test_and_set(4000))
        {
            switch (*voicecommand_spam)
            {
                case 1: // RANDOM
                    g_IEngine->ServerCmd(format("voicemenu ", UniformRandomInt(0, 2), " ", UniformRandomInt(0, 8)).c_str());
                    break;
                case 2: // MEDIC
                    g_IEngine->ServerCmd("voicemenu 0 0");
                    break;
                case 3: // THANKS
                    g_IEngine->ServerCmd("voicemenu 0 1");
                    break;
                case 4: // Go Go Go!
                    g_IEngine->ServerCmd("voicemenu 0 2");
                    break;
                case 5: // Move up!
                    g_IEngine->ServerCmd("voicemenu 0 3");
                    break;
                case 6: // Go left!
                    g_IEngine->ServerCmd("voicemenu 0 4");
                    break;
                case 7: // Go right!
                    g_IEngine->ServerCmd("voicemenu 0 5");
                    break;
                case 8: // Yes!
                    g_IEngine->ServerCmd("voicemenu 0 6");
                    break;
                case 9: // No!
                    g_IEngine->ServerCmd("voicemenu 0 7");
                    break;
                case 10: // Incoming!
                    g_IEngine->ServerCmd("voicemenu 1 0");
                    break;
                case 11: // Spy!
                    g_IEngine->ServerCmd("voicemenu 1 1");
                    break;
                case 12: // Sentry Ahead!
                    g_IEngine->ServerCmd("voicemenu 1 2");
                    break;
                case 13: // Need Teleporter Here!
                    g_IEngine->ServerCmd("voicemenu 1 3");
                    break;
                case 14: // Need Dispenser Here!
                    g_IEngine->ServerCmd("voicemenu 1 4");
                    break;
                case 15: // Need Sentry Here!
                    g_IEngine->ServerCmd("voicemenu 1 5");
                    break;
                case 16: // Activate Charge!
                    g_IEngine->ServerCmd("voicemenu 1 6");
                    break;
                case 17: // Help!
                    g_IEngine->ServerCmd("voicemenu 2 0");
                    break;
                case 18: // Battle Cry!
                    g_IEngine->ServerCmd("voicemenu 2 1");
                    break;
                case 19: // Cheers!
                    g_IEngine->ServerCmd("voicemenu 2 2");
                    break;
                case 20: // Jeers!
                    g_IEngine->ServerCmd("voicemenu 2 3");
                    break;
                case 21: // Positive!
                    g_IEngine->ServerCmd("voicemenu 2 4");
                    break;
                case 22: // Negative!
                    g_IEngine->ServerCmd("voicemenu 2 5");
                    break;
                case 23: // Nice shot!
                    g_IEngine->ServerCmd("voicemenu 2 6");
                    break;
                case 24: // Nice job!
                    g_IEngine->ServerCmd("voicemenu 2 7");
            }
        }
    }

    if (!spam_source)
        return;

    static int last_source  = 0;
    if (*spam_source != last_source)
        last_source  = *spam_source;

    const std::vector<std::string> *source = nullptr;
    switch (*spam_source)
    {
    case 1:
        source = &file.lines;
        break;
    case 2:
        source = &builtin_default;
        break;
    case 3:
        source = &builtin_lennyfaces;
        break;
    case 4:
        source = &builtin_nonecore;
        break;
    case 5:
        source = &builtin_lmaobox;
        break;
    case 6:
        source = &builtin_lithium;
        break;
    case 7:
        source = &builtin_pazer;
        break;
    default:
        return;
    }

    if (!source || !source->size())
        return;

    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - last_spam_point).count() > int(spam_delay))
    {
        if (chat_stack::stack.empty())
        {
            if (current_index >= source->size())
                current_index = 0;
            if (random_order && source->size() > 1)
            {
                current_index = UniformRandomInt(0, source->size() - 1);
                while (current_index == last_index)
                {
                    current_index = UniformRandomInt(0, source->size() - 1);
                }
            }
            last_index             = current_index;
            std::string spamString = source->at(current_index);
            if (FormatSpamMessage(spamString))
                chat_stack::Say(spamString, *team_only);
            current_index++;
        }
        last_spam_point = std::chrono::system_clock::now();
    }
}

void reloadSpamFile()
{
    file.Load(*filename);
}

bool isActive()
{
    return bool(spam_source);
}

void init()
{
    spam_source.installChangeCallback([](settings::VariableBase<int> &var, int after) { file.Load(*filename); });
    filename.installChangeCallback([](settings::VariableBase<std::string> &var, std::string after) { file.TryLoad(after); });
    reloadSpamFile();
}

const std::vector<std::string> builtin_default    = { "Cathook - more fun than a ball of yarn!", "GNU/Linux is the best OS!", "Visit https://github.com/nullworks/cathook for more information!", "Cathook - Free and Open-Source tf2 cheat!", "Cathook - ca(n)t stop me meow!" };
const std::vector<std::string> builtin_lennyfaces = { "( ͡° ͜ʖ ͡°)", "( ͡°( ͡° ͜ʖ( ͡° ͜ʖ ͡°)ʖ ͡°) ͡°)", "ʕ•ᴥ•ʔ", "(▀̿Ĺ̯▀̿ ̿)", "( ͡°╭͜ʖ╮͡° )", "(ง'̀-'́)ง", "(◕‿◕✿)", "༼ つ  ͡° ͜ʖ ͡° ༽つ" };
const std::vector<std::string> builtin_nonecore = { "NULL CORE - REDUCE YOUR RISK OF BEING OWNED!", "NULL CORE - WAY TO THE TOP!", "NULL CORE - BEST TF2 CHEAT!", "NULL CORE - NOW WITH BLACKJACK AND HOOKERS!", "NULL CORE - BUTTHURT IN 10 SECONDS FLAT!", "NULL CORE - WHOLE SERVER OBSERVING!", "NULL CORE - GET BACK TO PWNING!", "NULL CORE - WHEN PVP IS TOO HARDCORE!", "NULL CORE - CAN CAUSE KIDS TO RAGE!", "NULL CORE - F2P NOOBS WILL BE 100% NERFED!" };
const std::vector<std::string> builtin_lmaobox  = { "GET GOOD, GET LMAOBOX!", "LMAOBOX - WAY TO THE TOP", "WWW.LMAOBOX.NET - BEST FREE TF2 HACK!" };
const std::vector<std::string> builtin_lithium  = { "CHECK OUT www.YouTube.com/c/DurRud FOR MORE INFORMATION!", "PWNING AIMBOTS WITH OP ANTI-AIMS SINCE 2015 - LITHIUMCHEAT", "STOP GETTING MAD AND STABILIZE YOUR MOOD WITH LITHIUMCHEAT!", "SAVE YOUR MONEY AND GET LITHIUMCHEAT! IT IS FREE!", "GOT ROLLED BY LITHIUM? HEY, THAT MEANS IT'S TIME TO GET LITHIUMCHEAT!!" };
const std::vector<std::string> builtin_pazer    = { "Attention! There is a cheater on the other team named '%query:'. Please kick them!" };

static InitRoutine EC(
    []()
    {
        EC::Register(EC::CreateMove, CreateMove, "spam", EC::average);
        init();
    });

static CatCommand reload_cc("spam_reload", "Reload spam file", hacks::spam::reloadSpamFile);

} // namespace hacks::spam