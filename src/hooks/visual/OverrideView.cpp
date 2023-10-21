#include <MiscTemporary.hpp>
#include <settings/Float.hpp>
#include "HookedMethods.hpp"
static settings::Float override_fov_zoomed{ "visual.fov-zoomed", "0" };
static settings::Float override_fov{ "visual.fov", "0" };
static Timer update{};
namespace hooked_methods
{
DEFINE_HOOKED_METHOD(OverrideView, void, void *this_, CViewSetup *setup)
{
    original::OverrideView(this_, setup);
    if (!isHackActive() || g_Settings.bInvalid || CE_BAD(LOCAL_E))
        return;
    bool zoomed = setup->fov < CE_INT(LOCAL_E, netvar.iDefaultFOV) || g_pLocalPlayer->bZoomed;
    if (*no_zoom && zoomed)
    {
        auto fov                     = g_ICvar->FindVar("fov_desired");
        setup->fov                   = override_fov ? *override_fov : fov->GetInt();
        CE_INT(LOCAL_E, netvar.iFOV) = override_fov ? *override_fov : fov->GetInt();
    }
    else if (override_fov && !zoomed)
    {
        setup->fov = *override_fov;
    }

    if (spectator_target)
    {
        CachedEntity *spec = ENTITY(spectator_target);
        if (CE_GOOD(spec) && !CE_BYTE(spec, netvar.iLifeState))
        {
            setup->origin = spec->m_vecOrigin() + CE_VECTOR(spec, netvar.vViewOffset);
            // why not spectate yourself
            if (spec == LOCAL_E)
            {
                setup->angles = CE_VAR(spec, netvar.m_angEyeAnglesLocal, QAngle);
            }
            else
            {
                setup->angles = CE_VAR(spec, netvar.m_angEyeAngles, QAngle);
            }
        }
        if (g_IInputSystem->IsButtonDown(ButtonCode_t::KEY_SPACE))
        {
            spectator_target = 0;
        }
    }
    draw::fov = setup->fov;
}
} // namespace hooked_methods
