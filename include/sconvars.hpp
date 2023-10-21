#pragma once

#include "convar.h"

namespace sconvar
{
class SpoofedConVar
{
public:
    explicit SpoofedConVar(ConVar *var);
    ConVar *original;
    ConVar *spoof;
};
} // namespace sconvar
