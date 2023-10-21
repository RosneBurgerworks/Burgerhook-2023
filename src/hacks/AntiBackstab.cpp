#include <settings/Bool.hpp>
#include "common.hpp"
#include "hack.hpp"

namespace hacks::antibackstab
{

static settings::Boolean enable{ "antibackstab.enable", "false" };
bool noaa = false;

float GetAngle(CachedEntity *spy)
{
    float yaw, yaw2, anglediff;
    Vector diff;
    yaw             = g_pLocalPlayer->v_OrigViewangles.y;
    const Vector &A = LOCAL_E->m_vecOrigin();
    const Vector &B = spy->m_vecOrigin();
    diff            = (A - B);
    yaw2            = acos(diff.x / diff.Length()) * 180.0f / PI;
    if (diff.y < 0)
        yaw2 = -yaw2;
    anglediff = yaw - yaw2;
    if (anglediff > 180)
        anglediff -= 360;
    if (anglediff < -180)
        anglediff += 360;
    return anglediff;
}

static void CreateMove()
{
}
static InitRoutine EC(
    []()
    {
        EC::Register(EC::CreateMove, CreateMove, "antibackstab", EC::late);
        EC::Register(EC::CreateMoveWarp, CreateMove, "antibackstab_w", EC::late);
    });

} // namespace hacks::antibackstab
