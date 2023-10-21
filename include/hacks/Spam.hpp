#pragma once
#include <string>
#include <vector>

class CatCommand;

namespace hacks::spam
{
extern const std::vector<std::string> builtin_default;
extern const std::vector<std::string> builtin_lennyfaces;
extern const std::vector<std::string> builtin_nonecore;
extern const std::vector<std::string> builtin_lmaobox;
extern const std::vector<std::string> builtin_lithium;
extern const std::vector<std::string> builtin_pazer;

bool isActive();
void init();
} // namespace hacks::spam
