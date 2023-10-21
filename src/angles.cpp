#include "common.hpp"
#include "angles.hpp"

namespace angles
{

angle_data_s data_[PLAYER_ARRAY_SIZE];

void angle_data_s::push(const Vector &angle)
{
    if (not angle.x and not angle.y)
        return;
    good                = true;
    angles[angle_index] = angle;
    if (++angle_index >= count)
    {
        angle_index = 0;
    }

    if (angle_count < count)
    {
        angle_count++;
    }
}

float angle_data_s::deviation(int steps) const
{
    int j    = angle_index - 2;
    int k    = j + 1;
    float hx = 0, hy = 0;
    for (int i = 0; i < steps && i < angle_count; ++i)
    {
        if (j < 0)
            j = count + j;
        if (k < 0)
            k = count + k;

        float dev_x = std::abs(angles[k].x - angles[j].x);
        float dev_y = std::abs(angles[k].y - angles[j].y);
        if (dev_x > hx)
            hx = dev_x;
        if (dev_y > hy)
            hy = dev_y;

        --j;
        --k;
    }
    if (hy > 180)
        hy = 360 - hy;
    return sqrt(SQR(hx) + SQR(hy));
}

void Update()
{
    for (auto const &ent : entity_cache::player_cache)
    {
        auto &d = data_[ent->m_IDX];
        if (ent->m_IDX == g_IEngine->GetLocalPlayer())
        {
            d.push(current_user_cmd->viewangles);
        }
        else
        {
            d.push(CE_VECTOR(ent, netvar.m_angEyeAngles));
        }
    }
}
} // namespace angles
