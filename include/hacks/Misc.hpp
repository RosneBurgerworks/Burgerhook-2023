#pragma once

#include "config.h"

namespace hacks::misc
{
void generate_schema();
void Schema_Reload();
void CreateMove();
#if ENABLE_VISUALS
void DrawText();
#endif
int getCarriedBuilding();
extern int last_number;

extern float last_bucket;
} // namespace hacks::misc
