#include <PlayerTools.hpp>
#include "common.hpp"
#include "HookedMethods.hpp"
#include "MiscTemporary.hpp"
#include "Aimbot.hpp"

class Materials
{
public:
    CMaterialReference mat_dme_unlit;
    CMaterialReference mat_dme_lit;
    CMaterialReference mat_dme_unlit_overlay_base;
    CMaterialReference mat_dme_lit_overlay;

    // Sadly necessary ):
    CMaterialReference mat_dme_lit_fp;
    CMaterialReference mat_dme_unlit_overlay_base_fp;
    CMaterialReference mat_dme_lit_overlay_fp;

    void Shutdown()
    {
        mat_dme_unlit.Shutdown();
        mat_dme_lit.Shutdown();
        mat_dme_unlit_overlay_base.Shutdown();
        mat_dme_lit_overlay.Shutdown();

        mat_dme_lit_fp.Shutdown();
        mat_dme_unlit_overlay_base_fp.Shutdown();
        mat_dme_lit_overlay_fp.Shutdown();
    } 
};

namespace hooked_methods
{
static bool init_mat = false;
static Materials mats;

template <typename T> void rvarCallback(settings::VariableBase<T> &, T)
{
    init_mat = false;
}

class DrawEntry
{
public:
    int entidx;
    int parentidx;
    DrawEntry()
    {
    }
    DrawEntry(int own_idx, int parent_idx)
    {
        entidx    = own_idx;
        parentidx = parent_idx;
    }
};

DEFINE_HOOKED_METHOD(DrawModelExecute, void, IVModelRender *this_, const DrawModelState_t &state, const ModelRenderInfo_t &info, matrix3x4_t *bone)
{
    if (!isHackActive() || disable_visuals || CE_BAD(LOCAL_E))
        return original::DrawModelExecute(this_, state, info, bone);

    PROF_SECTION(DrawModelExecute);

    IClientUnknown *unk = info.pRenderable->GetIClientUnknown();
    if (unk)
    {
        IClientEntity *ent = unk->GetIClientEntity();
        if (ent && ent->entindex() == spectator_target)
            return;
    }
    return original::DrawModelExecute(this_, state, info, bone);
}
} // namespace hooked_methods
