#pragma once
#include "reclasses.hpp"
namespace re
{

class CTFPartyClient
{
public:
    static CTFPartyClient *GTFPartyClient();

    uintptr_t GetPartyObject();
    void SendPartyChat(const char *message);
    int LoadSavedCasualCriteria();
    static ITFGroupMatchCriteria *MutLocalGroupCriteria(CTFPartyClient *client);
    static bool BCanQueueForStandby(CTFPartyClient *this_);
    char RequestQueueForMatch(int type);
    void RequestQueueForStandby();
    bool BInQueueForStandby();
    bool BInQueueForMatchGroup(int type);
    char RequestLeaveForMatch(int type);
    int BInvitePlayerToParty(const CSteamID &steamid);
    int BRequestJoinPlayer(const CSteamID &steamid);
    static bool BInQueue(CTFPartyClient *this_);
    int GetNumOnlineMembers();
    int GetNumMembers();
    bool GetMemberID(unsigned idx, CSteamID &id);
    const CSteamID *GetMembersArray();
    int PromotePlayerToLeader(const CSteamID &steamid);
    std::vector<unsigned> GetPartySteamIDs();
    int KickPlayer(const CSteamID &steamid);
    bool GetCurrentPartyLeader(CSteamID &id);
    bool IsAnybodyBanned(bool competitive);
    bool IsMemberBanned(bool competitive, unsigned idx);
};
class ITFMatchGroupDescription
{
public:
    char pad0[4];
    int m_iID;
    char pad1[63];
    bool m_bForceCompetitiveSettings;
};

ITFMatchGroupDescription *GetMatchGroupDescription(int &idx);
} // namespace re
