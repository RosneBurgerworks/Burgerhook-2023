#include <optional>
#include "mathlib/vector.h"
#include "cdll_int.h"

#pragma once
class IClientEntity;

struct brutedata
{
    int brutenum{ 0 };
    int hits_in_a_row{ 0 };
    Vector original_angle{};
    Vector new_angle{};
};

namespace hacks::anti_anti_aim
{
extern boost::unordered_flat_map<unsigned, brutedata> resolver_map;
void increaseBruteNum(int idx);
void frameStageNotify(ClientFrameStage_t stage);
// void resolveEnt(int IDX, IClientEntity *entity = nullptr);
} // namespace hacks::anti_anti_aim
