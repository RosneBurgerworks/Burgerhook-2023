#pragma once

#include "reclasses.hpp"

namespace re
{

class CTFGCClientSystem
{
public:
    CTFGCClientSystem() = delete;
    static CTFGCClientSystem *GTFGCClientSystem();

public:
    void AbandonCurrentMatch();
    bool BConnectedToMatchServer(bool flag);
    bool BHaveLiveMatch();
    CTFParty *GetParty();
    int JoinMMMatch();
};
} // namespace re
