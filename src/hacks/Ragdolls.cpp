#include "common.hpp"
#include "sdk/dt_recv_redef.h"
#include "Ragdolls.hpp"

namespace hacks::ragdolls
{

static settings::Int mode{ "visual.ragdoll-mode", "1" };
static settings::Boolean only_local{ "visual.ragdoll-only-local", "false" };

    enum RagdollOverride_t
    {
        NONE = 0,
        GIB = 1,
    };

bool ragdollKillByLocal(void *ragdoll)
{
    int playerIndex = NET_INT(ragdoll, netvar.m_iPlayerIndex);
    if (playerIndex != g_pLocalPlayer->entity_idx)
    {
        auto owner = g_IEntityList->GetClientEntity(playerIndex);
        if (owner && !owner->IsDormant())
        {
            auto owner_observer = g_IEntityList->GetClientEntityFromHandle(NET_VAR(owner, netvar.hObserverTarget, CBaseHandle));
            return owner_observer && !owner_observer->IsDormant() && owner_observer->entindex() == g_pLocalPlayer->entity_idx;
        }
        return false;
    }
}
    ProxyFnHook gib_hook;

    void overrideGib(const CRecvProxyData* data, void* structure, void* out)
    {
        auto gib = reinterpret_cast<bool*>(out);
        if (*mode == RagdollOverride_t::GIB && (!*only_local || ragdollKillByLocal(structure)))
        {
            *gib = false;
        }
        else
        {
            *gib = data->m_Value.m_Int;
        }

        *gib = false;
    }

    void hook()
    {
        const char* ragdollTableName = "DT_TFRagdoll";
        const char* gibPropName = "m_bGib";

        for (auto dt_class = g_IBaseClient->GetAllClasses(); dt_class; dt_class = dt_class->m_pNext)
        {
            auto table = dt_class->m_pRecvTable;
            if (strcmp(table->m_pNetTableName, ragdollTableName) == 0)
            {
                for (int i = 0; i < table->m_nProps; ++i)
                {
                    auto prop = reinterpret_cast<RecvPropRedef*>(&table->m_pProps[i]);
                    if (prop && strcmp(prop->m_pVarName, gibPropName) == 0)
                    {
                        gib_hook.init(prop);
                        gib_hook.setHook(overrideGib);
                        return; // No need to continue looping through properties
                    }
                }
            }
        }
    }

void unhook()
{
    gib_hook.restore();
}

static InitRoutine init(
    []()
    {
        hook();
        EC::Register(EC::Shutdown, unhook, "ragdoll_shutdown");
    });

} // namespace hacks::ragdolls