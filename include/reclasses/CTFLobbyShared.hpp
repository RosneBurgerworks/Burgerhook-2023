#include "common.hpp"

class CTFLobbyPlayer {
public:
    /* Returns object value */
    // virtual CTFLobbyPlayer* Object() = 0;
    CTFLobbyPlayer *Object() { return object; }
    CSteamID GetID() { return GetThis<CSteamID>(0x10); }
    /* RED - 0; BLU - 1
     * Returns team info assigned to lobby
     * TO DO: Figure out if your team changes with autobalance
     */
    int GetTeam() { return GetThis<int>(0x18); }

private:
    template<class T>
    T GetThis(uintptr_t offset) {
        return *reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(this) + offset);
    }
    void *vt;
    CTFLobbyPlayer *object;
};

class CTFLobbyShared {
public:
    CTFLobbyShared()  = delete;
    ~CTFLobbyShared() = delete;

    /* Returns CTFLobbyShared or nullptr if it doesn't exist */
    static CTFLobbyShared *GetLobby();
    /* Same as GetMemberDetails but for pending players (not part of virtual table) */
    CTFLobbyPlayer GetPendingPlayerDetails(int idx);
    CTFLobbyPlayer *GetPlayer(int idx);
    CTFLobbyPlayer *GetPendingPlayer(int idx);

    /* Virtual table */
    virtual void Destructor0()                                           = 0;
    virtual void Destructor1()                                           = 0;
    virtual uint64_t GetGroupID() const                                  = 0;
    virtual int GetNumMembers() const                                    = 0;
    virtual CSteamID GetMember(int idx) const                            = 0;
    virtual int GetMemberIndexBySteamID(const CSteamID &id) const        = 0;
    virtual void *GetSharedObjectForMember(void *member)                 = 0;
    virtual void Dump() const                                            = 0;
    virtual CSteamID GetLeader() const                                   = 0; /* always return 0 */
    virtual int GetNumPendingPlayers() const                             = 0;
    virtual CSteamID GetPendingPlayer(int idx) const                     = 0;
    virtual CSteamID GetPendingPlayerInviter(int idx) const              = 0; /* always return 0 */
    virtual int GetPendingPlayerType(int idx) const                      = 0; /* always return 0 */
    virtual int GetPendingPlayerIndexBySteamID(const CSteamID &id) const = 0;
    virtual CTFLobbyPlayer GetMemberDetails(int idx)                     = 0;
    virtual void *GSObj() const                                          = 0;
    virtual void *GSObj()                                                = 0;
    /* Virtual table end */
};
