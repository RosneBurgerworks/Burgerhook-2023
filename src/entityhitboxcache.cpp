#include <settings/Int.hpp>
#include "common.hpp"
#include "MiscTemporary.hpp"
#include "SetupBonesReconst.hpp"

namespace hitbox_cache
{
void EntityHitboxCache::Init()
{
    if (m_bInit)
        return;

    parent_ref = &(entity_cache::array[hit_idx]);
    if (CE_BAD(parent_ref))
        return;

    const model_t* model = RAW_ENT(parent_ref)->GetModel();
    if (!model)
        return;

    if (!m_bModelSet || model != m_pLastModel)
    {
        studiohdr_t* shdr = g_IModelInfo->GetStudiomodel(model);
        if (!shdr)
            return;

        mstudiohitboxset_t* set = shdr->pHitboxSet(CE_INT(parent_ref, netvar.iHitboxSet));
        if (!set)
            return;

        m_pLastModel = const_cast<model_t*>(model);
        m_nNumHitboxes = std::min(set->numhitboxes, CACHE_MAX_HITBOXES);
        m_bModelSet = true;
    }

    m_bSuccess = true;
    m_bInit = true;
}

bool EntityHitboxCache::VisibilityCheck(int id)
{
    if (!m_bInit || id < 0 || id >= m_nNumHitboxes || !m_bSuccess)
        return false;

    if ((m_VisCheckValidationFlags >> id) & 1)
        return (m_VisCheck >> id) & 1;

    CachedHitbox* hitbox = GetHitbox(id);
    if (!hitbox)
        return false;

    bool validation = IsEntityVectorVisible(parent_ref, hitbox->center, true);
    uint_fast64_t mask = 1ULL << id;
    m_VisCheck = (m_VisCheck & ~mask) | (-validation & mask);
    m_VisCheckValidationFlags |= 1ULL << id;
    return (m_VisCheck >> id) & 1;
}

static settings::Int setupbones_time{ "source.setupbones-time", "4" };

void EntityHitboxCache::UpdateBones()
{
    if (!m_bInit)
        Init();

    auto bone_ptr = GetBones();
    if (!bone_ptr || bones.empty())
        return;

    struct BoneCache;
    typedef BoneCache* (*GetBoneCache_t)(unsigned);
    typedef void (*BoneCacheUpdateBones_t)(BoneCache*, matrix3x4_t* bones, unsigned, float time);

    static auto hitbox_bone_cache_handle_offset = *(unsigned*)(gSignatures.GetClientSignature("8B 86 ? ? ? ? 89 04 24 E8 ? ? ? ? 85 C0 89 C3 74 48") + 2);
    static auto studio_get_bone_cache = (GetBoneCache_t)gSignatures.GetClientSignature("55 89 E5 56 53 BB ? ? ? ? 83 EC 50 C7 45 D8");
    static auto bone_cache_update_bones = (BoneCacheUpdateBones_t)gSignatures.GetClientSignature("55 89 E5 57 31 FF 56 53 83 EC 1C 8B 5D 08 0F B7 53 10");

    auto hitbox_bone_cache_handle = CE_VAR(parent_ref, hitbox_bone_cache_handle_offset, unsigned);
    if (hitbox_bone_cache_handle)
    {
        BoneCache* bone_cache = studio_get_bone_cache(hitbox_bone_cache_handle);
        if (bone_cache && !bones.empty())
            bone_cache_update_bones(bone_cache, bones.data(), bones.size(), g_GlobalVars->curtime);
    }
}

matrix3x4_t *EntityHitboxCache::GetBones(int numbones)
{
    static float bones_setup_time = 0.0f;
    switch (*setupbones_time)
    {
    case 0:
        bones_setup_time = 0.0f;
        break;
    case 1:
        bones_setup_time = g_GlobalVars->curtime;
        break;
    case 2:
        if (CE_GOOD(LOCAL_E))
            bones_setup_time = SERVER_TIME;
        break;
    case 3:
        if (CE_GOOD(parent_ref))
            bones_setup_time = CE_FLOAT(parent_ref, netvar.m_flSimulationTime);
    }
    if (!bones_setup)
    {
        // Determine numbones if not provided
        if (numbones == -1)
        {
            numbones = (parent_ref->m_Type() == ENTITY_PLAYER) ? CE_INT(parent_ref, 0x844) : MAXSTUDIOBONES;
        }

        // Resize bones vector if needed
        if (bones.size() != static_cast<size_t>(numbones))
            bones.resize(numbones);

        if (g_Settings.is_create_move)
        {
            PROF_SECTION(bone_setup);
             // Only use reconstructed setupbones on players
            /*
            if (parent_ref->m_Type() == ENTITY_PLAYER)
                bones_setup = setupbones_reconst::SetupBones(RAW_ENT(parent_ref), bones.data(), 0x7FF00);
            else
                bones_setup = RAW_ENT(parent_ref)->SetupBones(bones.data(), numbones, 0x7FF00, bones_setup_time);
                */
            bones_setup = RAW_ENT(parent_ref)->SetupBones(bones.data(), numbones, BONE_USED_BY_HITBOX, bones_setup_time);
        }
    }
    return bones.data();
}

CachedHitbox *EntityHitboxCache::GetHitbox(int id)
{
    if ((m_CacheValidationFlags >> id) & 1)
        return &m_CacheInternal[id];

    if (!m_bInit || id < 0 || id >= m_nNumHitboxes || !m_bSuccess || CE_BAD(parent_ref))
        return nullptr;

    const model_t* model = RAW_ENT(parent_ref)->GetModel();
    if (!model)
        return nullptr;

    studiohdr_t* shdr = g_IModelInfo->GetStudiomodel(model);
    if (!shdr)
        return nullptr;

    mstudiohitboxset_t* set = shdr->pHitboxSet(CE_INT(parent_ref, netvar.iHitboxSet));
    if (!set)
        return nullptr;

    if (m_nNumHitboxes > m_CacheInternal.size())
        m_CacheInternal.resize(m_nNumHitboxes);

    mstudiobbox_t* box = set->pHitbox(id);
    if (!box || box->bone < 0 || box->bone >= MAXSTUDIOBONES)
        return nullptr;

    const matrix3x4_t* bonesMatrix = GetBones(shdr->numbones);
    if (!bonesMatrix)
        return nullptr;

    VectorTransform(box->bbmin, bonesMatrix[box->bone], m_CacheInternal[id].min);
    VectorTransform(box->bbmax, bonesMatrix[box->bone], m_CacheInternal[id].max);

    m_CacheInternal[id].bbox = box;
    m_CacheInternal[id].center = (m_CacheInternal[id].min + m_CacheInternal[id].max) * 0.5f;
    m_CacheValidationFlags |= 1ULL << id;
    return &m_CacheInternal[id];
}

} // namespace hitbox_cache
