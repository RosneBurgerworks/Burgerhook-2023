#include "reclasses.hpp"

CTFLobbyShared *CTFLobbyShared::GetLobby()
{
    static auto GCClient       = *reinterpret_cast<void **>(gSignatures.GetClientSignature("c7 04 ? ? ? ? ? 89 54 ? 08 89 44 ? 04 e8 ? ? ? ? 8b") + 0x3);
    static auto GetSharedLobby = reinterpret_cast<CTFLobbyShared *(*) (void *)>(gSignatures.GetClientSignature("55 89 e5 83 ec 18 8b ? ? 8b ? ? ? ? ? 85 c0 74 ? c7 44 ? 04 d4 07 00 00"));
    return GetSharedLobby(GCClient);
}

CTFLobbyPlayer *CTFLobbyShared::GetPlayer(int idx)
{
    return GetMemberDetails(idx).Object();
}

CTFLobbyPlayer *CTFLobbyShared::GetPendingPlayer(int idx)
{
    return GetPendingPlayerDetails(idx).Object();
}

CTFLobbyPlayer CTFLobbyShared::GetPendingPlayerDetails(int idx)
{
    static auto pGetPendingPlayerDetails = reinterpret_cast<CTFLobbyPlayer (*)(CTFLobbyShared *, int)>(gSignatures.GetClientSignature("55 89 e5 57 56 53 83 ec ? 8b ? ? 8b ? ? 8b ? ? 85 f6 "
                                                                                                                                      "78 ? 89 f2 89 f8 e8 ? ? ? ? 84 c0 74 ? 8b 07 89 3c ? "
                                                                                                                                      "ff 50 40 8b 40 7c 8b 04 b0 eb"));
    return pGetPendingPlayerDetails(this, idx);
}
