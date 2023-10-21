#include <chatlog.hpp>
#include <boost/algorithm/string.hpp>
#include <MiscTemporary.hpp>
#include <hacks/AntiAim.hpp>
#include <settings/Bool.hpp>
#include "HookedMethods.hpp"
#include "CatBot.hpp"
#include "ChatCommands.hpp"
#include <iomanip>
#include "votelogger.hpp"
#include "nospread.hpp"

static settings::Boolean dispatch_log{ "debug.log-dispatch-user-msg", "false" };
static settings::Boolean anti_autobalance{ "cat-bot.anti-autobalance", "false" };
static settings::Boolean anti_autobalance_safety{ "cat-bot.anti-autobalance.safety", "true" };

static bool retrun = false;

std::string lastfilter{};
std::string lastname{};

namespace hooked_methods
{
static std::string previous_name = "";
static Timer reset_it{};
static Timer wait_timer{};
void Paint()
{
    if (!wait_timer.test_and_set(1000))
        return;
    INetChannel *server = (INetChannel *) g_IEngine->GetNetChannelInfo();
    if (server)
        reset_it.update();
    if (reset_it.test_and_set(20000))
    {
        anti_balance_attempts = 0;
        previous_name         = "";
    }
}

template <typename T> void SplitName(std::vector<T> &ret, const T &name, int num)
{
    T tmp;
    int chars = 0;
    for (char i : name)
    {
        if (i == ' ')
            continue;

        tmp.push_back(std::tolower(i));
        ++chars;
        if (chars == num + 1)
        {
            chars = 0;
            ret.push_back(tmp);
            tmp.clear();
        }
    }
    if (tmp.size() > 2)
        ret.push_back(tmp);
}

static InitRoutine Autobalance([]() {EC::Register(EC::Paint, Paint, "paint_autobalance", EC::average);});

DEFINE_HOOKED_METHOD(DispatchUserMessage, bool, void *this_, int type, bf_read &buf)
{
    if (!isHackActive())
        return original::DispatchUserMessage(this_, type, buf);

    int s, i;
    char c;
    const char *buf_data = reinterpret_cast<const char *>(buf.m_pData);
    // We should bail out
    if (!hacks::nospread::DispatchUserMessage(&buf, type))
        return true;
    std::string data;
    switch (type)
    {
    case 12:
        if (*hacks::catbot::anti_motd && *hacks::catbot::catbotmode)
        {
            data = std::string(buf_data);
            if (data.find("class_") != std::string::npos)
                return false;
        }
        break;
    case 5:
    {
        if (*anti_autobalance && buf.GetNumBytesLeft() > 35)
        {
            INetChannel *server = (INetChannel *) g_IEngine->GetNetChannelInfo();
            data                = std::string(buf_data);
            logging::Info("%s", data.c_str());
            if (data.find("TeamChangeP") != data.npos && CE_GOOD(LOCAL_E))
            {
                std::string server_name(server->GetAddress());
                if (server_name != previous_name)
                {
                    previous_name         = server_name;
                    anti_balance_attempts = 0;
                }
                if (!anti_autobalance_safety || anti_balance_attempts < 2)
                {
                    logging::Info("Rejoin: anti autobalance");
                    ignoredc = true;
                    g_IEngine->ClientCmd_Unrestricted("killserver;wait 100;cat_mm_join");
                }
                else
                    re::CTFPartyClient::GTFPartyClient()->SendPartyChat("Will be autobalanced in 3 seconds");
                anti_balance_attempts++;
            }
            }
        }
    case 4:
    {
        s = buf.GetNumBytesLeft();
        if (s >= 256 || CE_BAD(LOCAL_E))
            break;

        for (i = 0; i < s; ++i)
            data.push_back(buf_data[i]);
        /* First byte is player ENT index
         * Second byte is unidentified (equals to 0x01)
         */
        const char *p = data.c_str() + 2;
        std::string event(p), name((p += event.size() + 1)), message(p + name.size() + 1);
         if (data[0] != LOCAL_E->m_IDX && event.find("TF_Chat") == 0)
        {
            player_info_s info{};
            GetPlayerInfo(LOCAL_E->m_IDX, &info);
            const char *claz  = nullptr;
            std::string name1 = info.name;

            switch (g_pLocalPlayer->clazz)
            {
            case tf_scout:
                claz = "scout";
                break;
            case tf_soldier:
                claz = "soldier";
                break;
            case tf_pyro:
                claz = "pyro";
                break;
            case tf_demoman:
                claz = "demo";
                break;
            case tf_engineer:
                claz = "engi";
                break;
            case tf_heavy:
                claz = "heavy";
                break;
            case tf_medic:
                claz = "med";
                break;
            case tf_sniper:
                claz = "sniper";
                break;
            case tf_spy:
                claz = "spy";
                break;
            default:
                break;
            }

            std::vector<std::string> res = { "skid", "script", "cheat", "hak", "hac", "f1", "hax", "vac", "ban", "bot", "report", "kick", "hcak", "chaet", "one" };
            if (claz)
                res.emplace_back(claz);

            SplitName(res, name1, 2);
            SplitName(res, name1, 3);

            std::string message2(message);
            boost::to_lower(message2);

            const char *toreplace[]   = { " ", "4", "3", "0", "6", "5", "7", "@", ".", ",", "-" };
            const char *replacewith[] = { "", "a", "e", "o", "g", "s", "t", "a", "", "", "" };

            for (int i = 0; i < 7; ++i)
                boost::replace_all(message2, toreplace[i], replacewith[i]);

        }
        if (event.find("TF_Chat") == 0)
            hacks::ChatCommands::handleChatMessage(message, data[0]);
        chatlog::LogMessage(data[0], message);
        buf = bf_read(data.c_str(), data.size());
        buf.Seek(0);
        break;
    }
    }
    votelogger::dispatchUserMessage(buf, type);
    return original::DispatchUserMessage(this_, type, buf);
}
} // namespace hooked_methods
