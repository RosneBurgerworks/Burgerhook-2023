#pragma once

#include <mathlib/vector.h>
#include <enums.hpp>

class CachedEntity;

class LocalPlayer
{
    unsigned long melee_damagetick = 0;

public:
    enum SpectatorState
    {
        NONE,
        ANY,
        FIRSTPERSON
    };

    // Start of CM
    void Update();
    // After prediction
    void UpdateEye();
    // End of CM
    void UpdateEnd();
    int team;
    int health;
    int flags;
    char life_state;
    int clazz;
    bool bZoomed;
    bool bRevving;
    bool bRevved;
    float flZoomBegin;
    bool holding_sniper_rifle;
    bool holding_sapper;
    weaponmode weapon_mode;
    bool using_action_slot_item{ false };

    Vector v_ViewOffset;
    Vector v_Origin;
    Vector v_Eye;
    int entity_idx;
    CachedEntity *entity{ nullptr };
    CachedEntity *weapon();
    bool weapon_melee_damage_tick;
    Vector v_OrigViewangles;
    Vector v_SilentAngles;
    bool bUseSilentAngles;
    bool bAttackLastTick;
    SpectatorState spectator_state;

    bool isFakeAngleCM = false;
    Vector realAngles{ 0.0f, 0.0f, 0.0f };
};

#define LOCAL_E g_pLocalPlayer->entity
#define LOCAL_W g_pLocalPlayer->weapon()

extern LocalPlayer *g_pLocalPlayer;
