#pragma once
#include "reclasses.hpp"

namespace re
{

class C_MannVsMachineStats
{
public:
    C_MannVsMachineStats() = delete;
    static C_MannVsMachineStats *G_MannVsMachineStats();

public:
    int *AddLocalPlayerUpgrade(int id, int &a3);
};
} // namespace re
