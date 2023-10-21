#pragma once

namespace hitrate
{
extern int count_shots;
extern int count_hits;
extern int count_hits_head;

void AimbotShot(int idx, bool target_body);
void Update();
} // namespace hitrate
