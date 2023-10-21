#include <MiscTemporary.hpp>
#include <hacks/hacklist.hpp>
#include <settings/Bool.hpp>
#include <hacks/Thirdperson.hpp>
#include "HookedMethods.hpp"
#include "hacks/Backtrack.hpp"
#include "AntiAntiAim.hpp"

static settings::Boolean no_shake{ "visual.no-shake", "false" };
namespace hooked_methods
{
#include "reclasses.hpp"
DEFINE_HOOKED_METHOD(FrameStageNotify, void, void *this_, ClientFrameStage_t stage)
{
    if (!isHackActive())
        return original::FrameStageNotify(this_, stage);

    PROF_SECTION(FrameStageNotify_TOTAL);

    if (!g_IEngine->IsInGame())
        g_Settings.bInvalid = true;
    {
        PROF_SECTION(FSN_antiantiaim);
        hacks::anti_anti_aim::frameStageNotify(stage);
    }
    std::optional<Vector> backup_punch;
    if (isHackActive() && !g_Settings.bInvalid && stage == FRAME_RENDER_START)
    {
        if (no_shake && CE_GOOD(LOCAL_E) && LOCAL_E->m_bAlivePlayer())
        {
            backup_punch                                       = NET_VECTOR(RAW_ENT(LOCAL_E), netvar.vecPunchAngle);
            NET_VECTOR(RAW_ENT(LOCAL_E), netvar.vecPunchAngle) = { 0.0f, 0.0f, 0.0f };
        }
        hacks::thirdperson::frameStageNotify();
    }
    original::FrameStageNotify(this_, stage);
    if (backup_punch)
        NET_VECTOR(RAW_ENT(LOCAL_E), netvar.vecPunchAngle) = *backup_punch;
}
} // namespace hooked_methods
