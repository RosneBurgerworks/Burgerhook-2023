#include <hacks/ESP.hpp>
#include <PlayerTools.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"

namespace hacks::esp
{

static settings::Boolean enable{ "esp.enable", "false" };
static settings::Int max_dist{ "esp.range", "1" };

static settings::Int box_esp{ "esp.box.mode", "1" };
static settings::Int box_corner_size_height{ "esp.box.corner-size.height", "10" };
static settings::Int box_corner_size_width{ "esp.box.corner-size.width", "10" };
static settings::Boolean box_3d_building{ "esp.box.building-3d", "false" };
static settings::Boolean bones_color{ "esp.bones.color", "false" };
static settings::Int healthbar{ "esp.health-bar", "0" };
static settings::Int sightlines{ "esp.sightlines", "0" };
static settings::Int esp_text_position{ "esp.text-position", "0" };
static settings::Int esp_expand{ "esp.expand", "0" };
static settings::Boolean vischeck{ "esp.vischeck", "true" };

static settings::Boolean local_esp{ "esp.show.local", "false" };
static settings::Boolean buildings{ "esp.show.buildings", "false" };
static settings::Boolean team_buildings{ "esp.show.team-buildings", "false" };
static settings::Boolean teammates{ "esp.show.teammates", "false" };
static settings::Boolean npc{ "esp.show.npc", "false" };

static settings::Boolean show_weapon{ "esp.info.weapon", "false" };
static settings::Boolean show_distance{ "esp.info.distance", "false" };
static settings::Boolean show_health{ "esp.info.health", "true" };
static settings::Boolean show_name{ "esp.info.name", "false" };
static settings::Boolean show_class{ "esp.info.class", "false" };
static settings::Boolean show_conditions{ "esp.info.conditions", "false" };
static settings::Boolean show_ubercharge{ "esp.info.ubercharge", "false" };
static settings::Boolean show_bot_id{ "esp.info.bot-id", "true" };

static settings::Boolean item_esp{ "esp.item.enable", "false" };
static settings::Boolean item_ammo_packs{ "esp.item.ammo", "false" };
static settings::Boolean item_health_packs{ "esp.item.health", "false" };

static settings::Boolean entity_info{ "esp.debug.entity", "false" };
static settings::Boolean entity_model{ "esp.debug.model", "false" };
static settings::Boolean entity_id{ "esp.debug.id", "false" };

// Forward declarations
void ResetEntityStrings(bool full_clear);

// Entity Processing
void __attribute__((fastcall)) ProcessEntity(CachedEntity *ent);
void __attribute__((fastcall)) ProcessEntityPT(CachedEntity *ent);
void __attribute__((fastcall)) hitboxUpdate(CachedEntity *ent);
// helper funcs
void __attribute__((fastcall)) Draw3DBox(CachedEntity *ent, const rgba_t &clr);
void __attribute__((fastcall)) DrawBox(CachedEntity *ent, const rgba_t &clr);
void BoxCorners(int minx, int miny, int maxx, int maxy, const rgba_t &color, bool transparent);
bool GetCollide(CachedEntity *ent);

// Storage vars for entities that need to be re-drawn
std::vector<std::pair<int, float>> entities_need_repaint{};

// Vars to store what bones connect to what
const std::string bonenames_leg_r[]  = { "bip_foot_R", "bip_knee_R", "bip_hip_R" };
const std::string bonenames_leg_l[]  = { "bip_foot_L", "bip_knee_L", "bip_hip_L" };
const std::string bonenames_bottom[] = { "bip_hip_R", "bip_pelvis", "bip_hip_L" };
const std::string bonenames_spine[]  = { "bip_pelvis", "bip_spine_0", "bip_spine_1", "bip_spine_2", "bip_spine_3", "bip_neck", "bip_head" };
const std::string bonenames_arm_r[]  = { "bip_upperArm_R", "bip_lowerArm_R", "bip_hand_R" };
const std::string bonenames_arm_l[]  = { "bip_upperArm_L", "bip_lowerArm_L", "bip_hand_L" };
const std::string bonenames_up[]     = { "bip_upperArm_R", "bip_spine_3", "bip_upperArm_L" };

class ESPString
{
public:
    std::string data;
    rgba_t color{ colors::empty };
};
class ESPData
{
public:
    int string_count{ 0 };
    std::array<ESPString, 16> strings{};
    rgba_t color{ colors::empty };
    bool needs_paint{ false };
    bool has_collide{ false };
    Vector collide_max{ 0, 0, 0 };
    Vector collide_min{ 0, 0, 0 };
    bool transparent{ false };
};

boost::unordered_flat_map<u_int16_t, ESPData> data;

// Dont fully understand struct but a guess is a group of something.
// I will return once I have enough knowlage to reverse this.
// NOTE: No idea on why we cant just use gethitbox and use the displacement on
// that insted of having all this extra code. Shouldnt gethitbox use cached
// hitboxes, if so it should be nicer on performance
// Use to add a esp string to an entity

inline void AddEntityString(CachedEntity *entity, const std::string &string, const rgba_t &color = colors::empty)
{
    ESPData &entity_data = data[entity->m_IDX];

    // Ensure that the entity's string count is less than 5
    if (entity_data.string_count < 5)
    {
        entity_data.strings[entity_data.string_count].data = string;
        entity_data.strings[entity_data.string_count].color = color;
        entity_data.string_count++;
        entity_data.needs_paint = true;
    }
}

class bonelist_s
{
private:
    bool setup{ false };
    bool success{ false };
    boost::unordered_flat_map<std::string, int> bones{};
    int leg_r[3]{ 0 };
    int leg_l[3]{ 0 };
    int bottom[3]{ 0 };
    int spine[7]{ 0 };
    int arm_r[3]{ 0 };
    int arm_l[3]{ 0 };
    int up[3]{ 0 };

public:
    void Setup(const studiohdr_t *hdr);
    void _FASTCALL DrawBoneList(const matrix3x4_t *bones, int *in, int size, const rgba_t &color);
    void _FASTCALL Draw(CachedEntity *ent, const rgba_t &color);
};

void bonelist_s::Setup(const studiohdr_t *hdr)
{
    if (!hdr)
    {
        setup   = false;
        success = false;
        return;
    }
    for (int i = 0; i < hdr->numbones; ++i)
        bones.emplace(std::make_pair(std::string(hdr->pBone(i)->pszName()), i));
    for (int i = 0; i < 7; ++i)
        spine[i] = bones.at(bonenames_spine[i]);
    for (int i = 0; i < 3; ++i)
    {
        arm_l[i]  = bones.at(bonenames_arm_l[i]);
        up[i]     = bones.at(bonenames_up[i]);
        arm_r[i]  = bones.at(bonenames_arm_r[i]);
        bottom[i] = bones.at(bonenames_bottom[i]);
        leg_l[i]  = bones.at(bonenames_leg_l[i]);
        leg_r[i]  = bones.at(bonenames_leg_r[i]);
    }

    success = true;
    setup   = true;
}

// These are strings that never change and should only be constructed once
const std::string dormant_str              = "*Dormant*";
const std::string jarated_str              = "*Jarate*";
const std::string taunting_str             = "*Taunt*";
const std::string revving_str              = "*Revved*";
const std::string slowed_str               = "*Slow*";
const std::string zooming_str              = "*Zoom*";
const std::string crit_str                 = "*Crits*";
const std::string blast_p_str              = "*Blast Passive*";
const std::string blast_a_str              = "*Blast-Resist*";
const std::string fire_p_str               = "*Fire Passive*";
const std::string fire_a_str               = "*Fire-Resist*";
const std::string bullet_p_str             = "*Bullet Passive*";
const std::string bullet_a_str             = "*Bullet-Resist*";
const std::string invulnerable_str         = "*Invulnerable*";
const std::string ready_ringer_str         = "*Dead Ringer Out*";
const std::string cloaked_str              = "*Cloak*";
const std::string in_ringer_str            = "*Dead Ringer*";
const std::string disguised_str            = "*Disguise*";
const std::string gassed_str               = "*Gassed*";
const std::string intel_str                = "Intel";
const std::string atombomb_str             = "Atom Bomb";
const std::string soulpickup_str           = "Soul Pickup";
const std::string bodyparts_str            = "Body Parts";
const std::string beerbottle_str           = "Beer Bottle";
const std::string gift_str                 = "Gift";
const std::string aussiecontainer_str      = "Australium Container";
const std::string ticketcase_str           = "Tickets";
const std::string mediumhealth_str         = "Medium Health";
const std::string smallhealth_str          = "Small Health";
const std::string cart_str                 = "Cart";
const std::string botname_str              = "Bot #";
const std::string tp_ready_str             = "Ready";
const std::string sapped_str               = "*Sapped*";
const std::string controlled_str           = "*Controlled*";
const std::string disabled_str             = "*Disabled*";
const std::string teleporter_str           = "Teleporter";
const std::string sentry_str               = "Sentry Gun";
const std::string dispenser_str            = "Dispenser";
const std::string rare_spell_str           = "Rare Spell";
const std::string spell_str                = "Spell";
const std::string ammo_big_str             = "Big Ammo";
const std::string ammo_medium_str          = "Medium Ammo";
const std::string ammo_small_str           = "Small Ammo";
const std::string health_big_str           = "Big Medkit";
const std::string health_medium_str        = "Medium Medkit";
const std::string health_small_str         = "Small Medkit";
const std::string mvm_money_str            = "Money";
const std::string mvm_red_money_str        = "~Money~";
const std::string tank_str                 = "Tank";
const std::string merasmus_str             = "Merasmus";
const std::string monoculus_str            = "Monoculus";
const std::string horsemann_str            = "Horsemann";
const std::string skeleton_str             = "Skeleton";
const std::string ghost_str                = "Ghost";
const std::string pumpkinbomb_str          = "Pumpkin Bomb";
const std::string crumpkin_str             = "Crumpkin";
const std::string gargoyle_str             = "Gargoyle";
const std::string balloonbomb_str          = "Dynamite Balloon";
const std::string woodenbarrel_str         = "Explosive Barrel";
const std::string walkerexplode_str        = "Alien Walker";
const std::string arrow_str                = "-->";
const std::string sticky_str               = "X";
const std::string pill_str                 = "<=>";
const std::string rocket_str               = "=>";

// Function called on draw
static void Draw()
{
    if (!enable)
        return;
    PROF_SECTION(DRAW_ESP_PERFORMANCE);
    for (auto &i : entities_need_repaint)
        ProcessEntityPT(ENTITY(i.first));
}

// Function called on create move
static void cm()
{
    // Check usersettings if enabled
    if (!*enable)
        return;
    if (CE_BAD(LOCAL_E))
        return;

    // Update entites every 1/5s
    const bool entity_tick = g_GlobalVars->tickcount % TIME_TO_TICKS(0.20f) == 0;

    ResetEntityStrings(entity_tick); // Clear any strings entities have
    entities_need_repaint.clear();   // Clear data on entities that need redraw
    int max_clients          = g_GlobalVars->maxClients;
    const bool vischeck_tick = g_GlobalVars->tickcount % TIME_TO_TICKS(0.50f) == 0;
    // If not using any other special esp, we lower the min to the max
    // clients
    if (!buildings && !item_esp)
    {

        // Do a vischeck every 1/2s

        { // Prof section ends when out of scope, these brackets here.
            PROF_SECTION(CM_ESP_EntityLoop);
            // Loop through entities

            for (auto const &ent : entity_cache::player_cache)
            {
                ProcessEntity(ent);
                hitboxUpdate(ent);
                ESPData &ent_dat = data.try_emplace(ent->m_IDX, ESPData{}).first->second;

                if (ent_dat.needs_paint)
                {
                    // Checking this every tick is a waste of nanoseconds
                    if (vischeck_tick && vischeck)
                        ent_dat.transparent = !ent->IsVisible();
                    entities_need_repaint.push_back({ ent->m_IDX, ent->m_vecOrigin().DistToSqr(g_pLocalPlayer->v_Origin) });
                }
            }
        }
    }
    else
    {
        { // Prof section ends when out of scope, these brackets here.
            PROF_SECTION(CM_ESP_EntityLoop);
            // Loop through entities
            for (auto const &ent_index : entity_cache::valid_ents)
            {
                // Get an entity from the loop tick and process it
                if (!ent_index->m_bAlivePlayer())
                    continue;

                bool player = ent_index->m_IDX < max_clients;

                if (player)
                {
                    ProcessEntity(ent_index);
                    hitboxUpdate(ent_index);
                }
                else if (entity_tick)
                {
                    ProcessEntity(ent_index);
                    hitboxUpdate(ent_index);
                }
                ESPData &ent_data = data.try_emplace(ent_index->m_IDX, ESPData{}).first->second;

                if (ent_data.needs_paint)                
                {
                    // Checking this every tick is a waste of nanoseconds
                    if (vischeck_tick && vischeck)
                        ent_data.transparent = !ent_index->IsVisible();
                    entities_need_repaint.push_back({ ent_index->m_IDX, ent_index->m_vecOrigin().DistToSqr(g_pLocalPlayer->v_Origin) });
                }
            }
        }
    }
    // Render closer entities later in order to have their text in the foreground
    std::sort(entities_need_repaint.begin(), entities_need_repaint.end(), [](std::pair<int, float> &a, std::pair<int, float> &b) { return a.second > b.second; });
}

// This is used to stop the bone ESP from lagging
void _FASTCALL hitboxUpdate(CachedEntity *ent)
{
    // Check to prevent crashes
    if (CE_BAD(ent) || !ent->m_bAlivePlayer())
        return;
    if (ent->m_Type() == ENTITY_PLAYER)
    {
        auto hit = ent->hitboxes.GetHitbox(0);
        if (!hit)
            return;
        Vector hbm, hbx;
        if (draw::WorldToScreen(hit->min, hbm) && draw::WorldToScreen(hit->max, hbx))
            Vector head_scr;
    }
}

void _FASTCALL Sightlines(CachedEntity *ent, rgba_t &fg)
{

    // Logic for using the enum to sort out snipers
    if (((int) sightlines == 2 || ((int) sightlines == 1 && CE_INT(ent, netvar.iClass) == tf_sniper)) && CE_GOOD(ent) && ent->hitboxes.GetHitbox(0))
    {
        PROF_SECTION(PT_esp_sightlines);

        // Get players angle and head position
        Vector &eye_angles = NET_VECTOR(RAW_ENT(ent), netvar.m_angEyeAngles);
        Vector eye_position;
        eye_position = ent->hitboxes.GetHitbox(0)->center;

        // Main ray tracing area
        float sy         = sinf(DEG2RAD(eye_angles.y)); // yaw
        float cy         = cosf(DEG2RAD(eye_angles.y));
        float sp         = sinf(DEG2RAD(eye_angles.x)); // pitch
        float cp         = cosf(DEG2RAD(eye_angles.x));
        Vector forward_t = Vector(cp * cy, cp * sy, -sp);
        // We dont want the sightlines endpoint to go behind us because the
        // world to screen check will fail, but keep it at most 4096
        Vector forward = forward_t * 4096.0F + eye_position;
        Ray_t ray;
        ray.Init(eye_position, forward);
        trace_t trace;
        g_ITrace->TraceRay(ray, MASK_SOLID, &trace::filter_no_player, &trace);

        // Screen vectors
        Vector scn1, scn2;

        // Status vars
        bool found_scn2 = true;

        // Get end point on screen
        if (!draw::WorldToScreen(trace.endpos, scn2))
        {
            // Set status
            found_scn2 = false;
            // Get the end distance from the trace
            float end_distance = trace.endpos.DistTo(eye_position);

            // Loop and look back until we have a vector on screen
            for (int i = 1; i < 500; ++i)
            {
                // Subtract 40 multiplyed by the tick from the end distance
                // and use that as our length to check
                Vector end_vector = forward_t * (end_distance - (10 * i)) + eye_position;
                if (end_vector.DistTo(eye_position) < 1)
                    break;
                if (draw::WorldToScreen(end_vector, scn2))
                {
                    found_scn2 = true;
                    break;
                }
            }
        }

        if (found_scn2)
        {
            // Set status
            bool found_scn1 = true;

            // If we dont have a vector on screen, attempt to find one
            if (!draw::WorldToScreen(eye_position, scn1))
            {
                // Set status
                found_scn1 = false;

                // Loop and look back untill we have a vector on screen
                for (int i = 1; i < 500; ++i)
                {
                    // Multiply starting distance by 15, multiplyed by the
                    // loop tick
                    Vector start_vector = forward_t * (10 * i) + eye_position;
                    // We dont want it to go too far
                    if (start_vector.DistTo(trace.endpos) < 1)
                        break;
                    // Check if we have a vector on screen, if we do then we
                    // set our status
                    if (draw::WorldToScreen(start_vector, scn1))
                    {
                        found_scn1 = true;
                        break;
                    }
                }
            }
            // We have both vectors, draw
            if (found_scn1)
            {
                draw::Line(scn1.x, scn1.y, scn2.x - scn1.x, scn2.y - scn1.y, fg, 0.5f);
            }
        }
    }
}

void _FASTCALL Healthbar(EntityType &type, int &classid, rgba_t &fg, ESPData &ent_data, CachedEntity *ent)
{

    if (type == ENTITY_PLAYER || type == ENTITY_BUILDING)
    {
        // Get collidable from the cache
        if (GetCollide(ent))
        {

            // Pull the cached collide info
            int max_x = ent_data.collide_max.x;
            int max_y = ent_data.collide_max.y;
            int min_x = ent_data.collide_min.x;
            int min_y = ent_data.collide_min.y;

            // Get health values
            int health    = 0;
            int healthmax = 0;
            switch (type)
            {
            case ENTITY_PLAYER:
                health    = g_pPlayerResource->GetHealth(ent);
                healthmax = g_pPlayerResource->GetMaxHealth(ent);
                break;
            case ENTITY_BUILDING:
                health    = CE_INT(ent, netvar.iBuildingHealth);
                healthmax = CE_INT(ent, netvar.iBuildingMaxHealth);
                break;
            }

            // Get Colors
            rgba_t hp     = colors::Transparent(colors::Health(health, healthmax), fg.a);
            rgba_t border = ((classid == RCC_PLAYER) && IsPlayerInvisible(ent)) ? colors::FromRGBA8(160, 160, 160, fg.a * 255.0f) : colors::Transparent(colors::black, fg.a);
            // Get bar width and height
            int hbw = (max_x - min_x - 1) * std::min((float) health / (float) healthmax, 1.0f);
            int hbh = (max_y - min_y - 2) * std::min((float) health / (float) healthmax, 1.0f);

            // Top horizontal health bar
            if (*healthbar == 1)
            {
                draw::RectangleOutlined(min_x, min_y - 6, max_x - min_x + 1, 7, border, 0.5f);
                draw::Rectangle(min_x + hbw, min_y - 5, -hbw, 5, hp);
            }
            // Bottom horizontal health bar
            else if (*healthbar == 2)
            {
                draw::RectangleOutlined(min_x, max_y, max_x - min_x + 1, 7, border, 0.5f);
                draw::Rectangle(min_x + hbw, max_y + 1, -hbw, 5, hp);
            }
            // Vertical health bar
            else if (*healthbar == 3)
            {
                draw::RectangleOutlined(min_x - 7, min_y, 7, max_y - min_y, border, 0.5f);
                draw::Rectangle(min_x - 6, max_y - hbh - 1, 5, hbh, hp);
            }
        }
    }
}

void DrawStrings(EntityType &type, bool &transparent, Vector &draw_point, ESPData &ent_data, CachedEntity *ent)
{
    PROF_SECTION(PT_esp_drawstrings);

    // Create our initial point at the center of the entity

    bool origin_is_zero = true;

    // Only get collidable for players and buildings
    if (type == ENTITY_PLAYER || type == ENTITY_BUILDING)
    {

        // Get collidable from the cache
        if (GetCollide(ent))
        {

            // Origin could change so we set to false
            origin_is_zero = false;

            // Pull the cached collide info
            int max_x = ent_data.collide_max.x;
            int max_y = ent_data.collide_max.y;
            int min_x = ent_data.collide_min.x;
            int min_y = ent_data.collide_min.y;

            // Change the position of the draw point depending on the user
            // settings
            switch ((int) esp_text_position)
            {
            case 0:
            { // TOP RIGHT
                draw_point = Vector(max_x + 2, min_y, 0);
            }
            break;
            case 1:
            { // BOTTOM RIGHT
                draw_point = Vector(max_x + 2, max_y - data.at(ent->m_IDX).string_count * 16, 0);
            }
            break;
            case 2:
            {                          // CENTER
                origin_is_zero = true; // origin is still zero so we set to true
            }
            break;
            case 3:
            { // ABOVE CENTER
                draw_point = Vector((min_x + max_x) / 2.0f, min_y - data.at(ent->m_IDX).string_count * 16, 0);
            }
            break;
            case 4:
            { // BELOW
                draw_point = Vector((min_x + max_x) / 2.0f, max_y, 0);
            }
            break;
            case 5:
            { // ABOVE LEFT
                draw_point = Vector(min_x + 2, min_y - data.at(ent->m_IDX).string_count * 16, 0);
            }
            break;
            case 6:
            { // ABOVE RIGHT
                draw_point = Vector(max_x + 2, min_y - data.at(ent->m_IDX).string_count * 16, 0);
            }
            }
        }
    }

    // Loop through strings
    for (int j = 0; j < ent_data.string_count; ++j)
    {

        // Pull string from the entity's cached string array
        const ESPString &string = ent_data.strings[j];

        // If string has a color assined to it, apply that otherwise use
        // entities color
        rgba_t color = string.color ? string.color : ent_data.color;
        if (transparent)
            color = colors::Transparent(color); // Apply transparency if needed

        // If the origin is centered, we use one method. if not, the other
        if (!origin_is_zero || true)
        {
            float draw_pointx_tmp = draw_point.x;
            // Above/Below text should be centered
            if (*esp_text_position == 3 || *esp_text_position == 4)
            {
                float w, h;
                fonts::esp->stringSize(string.data, &w, &h);
                draw_pointx_tmp -= w / 2.0f;
            }
            draw::String(draw_pointx_tmp, draw_point.y, color, string.data.c_str(), *fonts::esp);
        }

        // Add to the y due to their being text in that spot
        draw_point.y += /*((int)fonts::font_main->height)*/ 15 - 1;
    }
}

void _FASTCALL BoxEsp(EntityType &type, bool &transparent, rgba_t &fg, CachedEntity *ent)
{
    switch (type)
    {
    case ENTITY_PLAYER:
        if (!fg)
            fg = colors::EntityF(ent);
        if (transparent)
            fg = colors::Transparent(fg);
        if (RAW_ENT(ent)->IsDormant())
        {
            fg.r *= 0.75f;
            fg.g *= 0.75f;
            fg.b *= 0.75f;
        }
    case ENTITY_BUILDING:
        if (CE_INT(ent, netvar.iTeamNum) == g_pLocalPlayer->team && !team_buildings)
            break;
        if (!fg)
            fg = colors::EntityF(ent);
        if (transparent)
            fg = colors::Transparent(fg);
        if (RAW_ENT(ent)->IsDormant())
        {
            fg.r *= 0.75f;
            fg.g *= 0.75f;
            fg.b *= 0.75f;
        }
        // Draw exit arrow
        // YawToExit is 0.0f on exit and on newly placed still disabled entrances
        // m_iState is 0 when the TP is disabled
        if (ent->m_iClassID() == CL_CLASS(CObjectTeleporter) && CE_FLOAT(ent, netvar.m_flTeleYawToExit) == 0.0f && CE_INT(ent, netvar.m_iTeleState) > 1)
        {
            float sin_a, cos_a;
            // for some reason vAngRotation yaw differs from exit direction, unsure why
            SinCos(DEG2RAD(CE_VECTOR(ent, netvar.m_angRotation).y - 90.0f), &sin_a, &cos_a);

            // pseudo used to rotate properly
            // screen is passed to filledpolygon
            Vector pseudo[3];
            Vector screen[3];

            // 24 = teleporter width and height
            // 16 = arrow size
            pseudo[0].x = 0.0f; // Undefined behaviour bruh
            pseudo[0].y = 16.0f + 24.0f;
            pseudo[1].x = -16.0f;
            pseudo[1].y = 24.0f;
            pseudo[2].x = 16.0f;
            pseudo[2].y = 24.0f;

            // pass vector, get vector
            // 12 is teleporter height, arrow is 12 HU off the ground
            // sin and cos are already passed in captures
            auto rotateVector = [sin_a = sin_a, cos_a = cos_a](Vector &in) { return Vector(in.x * cos_a - in.y * sin_a, in.x * sin_a + in.y * cos_a, 12.0f); };

            // fail check
            bool visible = true;

            // rotate, add vecorigin AND check worldtoscreen at the same time in single loop
            for (int p = 0; p < 3; ++p)
                if (!(visible = draw::WorldToScreen(rotateVector(pseudo[p]) + ent->m_vecOrigin(), screen[p])))
                    break;

            // if visible, pass it and draw the whole thing, ez pz
            if (visible)
                draw::Triangle(screen[0].x, screen[0].y, screen[1].x, screen[1].y, screen[2].x, screen[2].y, fg);
        }
        if (!box_3d_building && box_esp)
            DrawBox(ent, fg);
        else if (box_3d_building)
            Draw3DBox(ent, fg);
        break;
    }
}

// Used when processing entitys with cached data from createmove in draw
void _FASTCALL ProcessEntityPT(CachedEntity *ent)
{
    PROF_SECTION(PT_esp_process_entity);

    // Check to prevent crashes
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return;
    // Dormant
    bool dormant = false;
    if (RAW_ENT(ent)->IsDormant())
    {
        if (!ent->m_vecDormantOrigin())
            return;
        dormant = true;
    }

    int classid     = ent->m_iClassID();
    EntityType type = ent->m_Type();
    // Grab esp data
    ESPData &ent_data = data[ent->m_IDX];

    // Get color of entity
    // TODO, check if we can move this after world to screen check
    rgba_t fg = ent_data.color;
    if (!fg || fg.a == 0.0f)
    {
        fg = colors::EntityF(ent);
        if (dormant)
        {
            fg.r *= 0.75f;
            fg.g *= 0.75f;
            fg.b *= 0.75f;
        }
        ent_data.color = fg;
    }

    // Check if entity is on screen, then save screen position if true
    auto position = ent->m_vecDormantOrigin();
    if (!position)
        return;

    // Sightline esp
    if (sightlines && type == ENTITY_PLAYER)
        Sightlines(ent, fg);

    static Vector screen;
    if (!draw::EntityCenterToScreen(ent, screen))
        return;

    // Reset the collide cache
    ent_data.has_collide = false;

    // Get if ent should be transparent
    bool transparent = vischeck && ent_data.transparent;

    // Box esp
    if (box_esp || box_3d_building)
        BoxEsp(type, transparent, fg, ent);

    // Health bar
    if (*healthbar != 0)
        Healthbar(type, classid, fg, ent_data, ent);
    // We only want health bars on players and buildings

    // Check if entity has strings to draw
    if (ent_data.string_count)
        DrawStrings(type, transparent, screen, ent_data, ent);
}

// Used to process entities from CreateMove
void _FASTCALL ProcessEntity(CachedEntity *ent)
{
    auto origin = ent->m_vecDormantOrigin();
    // Dormant and no data
    if (!origin)
        return;

    {
        // We don't actually care about this vector at all. It just exists so WorldToScreen can function normally
        static Vector origin_screen;
        if (!sightlines && ent->m_Type() != ENTITY_PLAYER && !draw::WorldToScreen(*origin, origin_screen))
            return;
    }

    auto distance = ent->m_flDistance();

    if (max_dist && distance > *max_dist)
        return;
    int classid = ent->m_iClassID();
    // Entity esp
    if (entity_info)
    {
        AddEntityString(ent, format(RAW_ENT(ent)->GetClientClass()->m_pNetworkName, " [", classid, "]"));
        if (entity_id)
        {
            AddEntityString(ent, std::to_string(ent->m_IDX));
        }
        if (entity_model)
        {
            const model_t *model = RAW_ENT(ent)->GetModel();
            if (model)
                AddEntityString(ent, std::string(g_IModelInfo->GetModelName(model)));
        }
    }

    // Get esp data from current ent
    ESPData &espdata = data[ent->m_IDX];

    int itemtype = ent->m_ItemType();
    // NPC esp
    if (npc)
    {
        // We can mark everything except the ghost like this
        if (ent->m_Type() == ENTITY_NPC)
        {
            switch (classid)
            {
            case CL_CLASS(CTFTankBoss):
                AddEntityString(ent, tank_str, colors::FromRGBA8(0, 128, 0, 255));
                break;
            case CL_CLASS(CMerasmus):
            case CL_CLASS(CMerasmusDancer):
                AddEntityString(ent, merasmus_str, colors::FromRGBA8(0, 128, 0, 255));
                break;
            case CL_CLASS(CZombie):
                AddEntityString(ent, skeleton_str, colors::FromRGBA8(0, 128, 0, 255));
                break;
            case CL_CLASS(CEyeballBoss):
                AddEntityString(ent, monoculus_str, colors::FromRGBA8(0, 128, 0, 255));
                break;
            case CL_CLASS(CHeadlessHatman):
                AddEntityString(ent, horsemann_str, colors::FromRGBA8(0, 128, 0, 255));
                break;
            }
        }
        else if (itemtype == HALLOWEEN_GHOST)
            AddEntityString(ent, ghost_str, colors::FromRGBA8(0, 128, 0, 255));
    }
    if (item_esp)
    {
        {
            // Health pack esp
            if (item_health_packs && ((itemtype >= ITEM_HEALTH_SMALL && itemtype <= EDIBLE_MEDIUM)))
            {
                switch (itemtype)
                {
                case ITEM_HEALTH_SMALL:
                    AddEntityString(ent, health_small_str);
                    break;
                case ITEM_HEALTH_MEDIUM:
                    AddEntityString(ent, health_medium_str);
                    break;
                case ITEM_HEALTH_LARGE:
                    AddEntityString(ent, health_big_str);
                    break;
                case EDIBLE_MEDIUM:
                    AddEntityString(ent, mediumhealth_str, colors::green);
                    break;
                case EDIBLE_SMALL:
                    AddEntityString(ent, smallhealth_str, colors::green);
                    break;
                }
            }
            else if (item_ammo_packs && itemtype >= ITEM_AMMO_SMALL && itemtype <= ITEM_AMMO_LARGE)
            {
                switch (itemtype)
                {
                case ITEM_AMMO_SMALL:
                    AddEntityString(ent, ammo_small_str);
                    break;
                case ITEM_AMMO_MEDIUM:
                    AddEntityString(ent, ammo_medium_str);
                    break;
                case ITEM_AMMO_LARGE:
                    AddEntityString(ent, ammo_big_str);
                    break;
                }
            }
        }
    }
    // Building esp
    else if (buildings && ent->m_Type() == ENTITY_BUILDING)
    {

        // Check if enemy building
        if (!team_buildings && !ent->m_bEnemy())
            return;

        // Make a name for the building based on the building type and level
        if (show_name || show_class)
        {
            const std::string &name = (classid == CL_CLASS(CObjectTeleporter) ? teleporter_str : (classid == CL_CLASS(CObjectSentrygun) ? sentry_str : dispenser_str));
            int level               = CE_INT(ent, netvar.iUpgradeLevel);
            bool IsMini             = CE_BYTE(ent, netvar.m_bMiniBuilding);

            if (!IsMini)
                AddEntityString(ent, format(name, " (Level ", level, ")"));
            else
                AddEntityString(ent, std::string("Mini ") + name);
        }

        // If text health is true, then add a string with the health
        if (show_health)
        {
            AddEntityString(ent, format(ent->m_iHealth(), '/', ent->m_iMaxHealth(), " HP"), colors::Health(ent->m_iHealth(), ent->m_iMaxHealth()));
        }

        if (show_conditions)
        {
            bool IsSapped         = CE_BYTE(ent, netvar.m_bHasSapper);
            bool IsDisabled       = CE_BYTE(ent, netvar.m_bDisabled);
            bool IsPlasmaDisabled = CE_BYTE(ent, netvar.m_bPlasmaDisable);
            bool IsMini           = CE_BYTE(ent, netvar.m_bMiniBuilding);
            int required_metal    = CE_INT(ent, netvar.m_iUpgradeMetalRequired);
            int metal             = CE_INT(ent, netvar.m_iUpgradeMetal);

            if (!IsMini && CE_INT(ent, netvar.iUpgradeLevel) != 3)
                AddEntityString(ent, format("Upgrade: " + std::to_string(required_metal - metal), '/', std::to_string(required_metal)));

            switch (classid)
            {
            case CL_CLASS(CObjectSentrygun):
            {
                bool IsControlled  = CE_BYTE(ent, netvar.m_bPlayerControlled);
                int sentry_ammo    = CE_INT(ent, netvar.m_iAmmoShells);
                int sentry_rockets = CE_INT(ent, netvar.m_iAmmoRockets);
                int max_ammo       = 0;

                switch (CE_INT(ent, netvar.iUpgradeLevel))
                {
                case 1:
                    max_ammo = 150;
                    break;
                case 2:
                case 3:
                    max_ammo = 200;
                    break;
                }

                AddEntityString(ent, format(std::to_string(sentry_ammo), '/', max_ammo, " Ammo"));

                if (CE_INT(ent, netvar.iUpgradeLevel) == 3)
                    AddEntityString(ent, format(std::to_string(sentry_rockets), '/', "20 Rockets"));

                if (IsControlled) // Dispensers are also "controlled" when they're in use
                    AddEntityString(ent, controlled_str, colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
                break;
            }
            case CL_CLASS(CObjectDispenser):
            {
                int AmmoMetal = CE_INT(ent, netvar.m_iAmmoMetal);

                AddEntityString(ent, format(std::to_string(AmmoMetal), '/', "400 Metal"));
                break;
            }
            case CL_CLASS(CObjectTeleporter):
            {
                if (CE_INT(ent, netvar.m_iTeleState) > 1)
                {
                    float next_teleport = CE_FLOAT(ent, netvar.m_flTeleRechargeTime);
                    float yaw_to_exit   = CE_FLOAT(ent, netvar.m_flTeleYawToExit);
                    std::string time    = std::to_string(int(SERVER_TIME - next_teleport));
                    time.append("s");

                    if (yaw_to_exit)
                    {
                        if (SERVER_TIME > next_teleport)
                            AddEntityString(ent, tp_ready_str);
                        else
                            AddEntityString(ent, time);
                    }
                }
                break;
            }
            default:
                break;
            }

            if (IsSapped)
                AddEntityString(ent, sapped_str, colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));

            else if ((classid == CL_CLASS(CObjectTeleporter) && CE_INT(ent, netvar.m_iTeleState) <= 1) || IsDisabled || IsPlasmaDisabled)
                AddEntityString(ent, disabled_str, colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
        }

        // Set the entity to repaint
        espdata.needs_paint = true;

        // Player esp
    }
    else if (ent->m_Type() == ENTITY_PLAYER && ent->m_bAlivePlayer())
    {
        // Local player handling
        if (!(local_esp && g_IInput->CAM_IsThirdPerson()) && ent->m_IDX == g_IEngine->GetLocalPlayer())
            return;
        if (IsPlayerInvisible(ent))
            return;

        // Get player class
        int pclass = CE_INT(ent, netvar.iClass);

        // Attempt to get player info, and if cant, return
        player_info_s info;
        if (!GetPlayerInfo(ent->m_IDX, &info))
            return;

        if (ent->m_bEnemy() || teammates || player_tools::shouldAlwaysRenderEsp(ent))
        {
            // Playername
            if (show_name)
                AddEntityString(ent, std::string(info.name));

            // Player class
            if (show_class && pclass > 0 && pclass < 10)
                AddEntityString(ent, classes[pclass - 1]);

#if ENABLE_IPC
            // ipc bot esp
            if (show_bot_id && ipc::peer && ent != LOCAL_E)
            {
                for (unsigned i = 0; i < cat_ipc::max_peers; ++i)
                {
                    if (!ipc::peer->memory->peer_data[i].free && ipc::peer->memory->peer_user_data[i].friendid == info.friendsID)
                    {
                        AddEntityString(ent, botname_str + std::to_string(i));
                        break;
                    }
                }
            }
#endif
            // Health esp
            if (show_health)
            {
                int health     = g_pPlayerResource->GetHealth(ent);
                int max_health = g_pPlayerResource->GetMaxHealth(ent);
                AddEntityString(ent, format(health, '/', max_health, " HP"), colors::Health(health, max_health));
            }
            // Medigun Ubercharge esp
            if (show_ubercharge)
            {
                if (CE_INT(ent, netvar.iClass) == tf_medic)
                {
                    int *weapon_list = (int *) ((unsigned) (RAW_ENT(ent)) + netvar.hMyWeapons);
                    for (int i = 0; weapon_list[i]; ++i)
                    {
                        int handle = weapon_list[i];
                        int eid    = HandleToIDX(handle);
                        if (eid > MAX_PLAYERS && eid <= HIGHEST_ENTITY)
                        {
                            CachedEntity *weapon = ENTITY(eid);
                            if (!CE_INVALID(weapon) && weapon->m_iClassID() == CL_CLASS(CWeaponMedigun) && weapon)
                            {
                                std::string charge = std::to_string(int(CE_FLOAT(weapon, netvar.m_flChargeLevel) * 100));

                                if (CE_INT(weapon, netvar.iItemDefinitionIndex) != 998)
                                {
                                    AddEntityString(ent, charge + "% Uber", colors::Health(CE_FLOAT(weapon, netvar.m_flChargeLevel) * 100, 100));
                                }
                                else
                                    AddEntityString(ent, charge + "% Uber | Charges: " + std::to_string(int(CE_FLOAT(weapon, netvar.m_flChargeLevel) / 0.25f)), colors::Health((CE_FLOAT(weapon, netvar.m_flChargeLevel) * 100), 100));
                                break;
                            }
                        }
                    }
                }
            }
            // Conditions esp
            if (show_conditions)
            {
                auto clr = colors::EntityF(ent);
                if (RAW_ENT(ent)->IsDormant())
                {
                    clr.r *= 0.5f;
                    clr.g *= 0.5f;
                    clr.b *= 0.5f;
                }
                // Invis
                if (IsPlayerInvisible(ent))
                {
                    if (HasCondition<TFCond_DeadRingered>(ent))
                        AddEntityString(ent, in_ringer_str, colors::FromRGBA8(178.0f, 0.0f, 255.0f, 255.0f));
                    else
                        AddEntityString(ent, cloaked_str, colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
                }
                if (CE_BYTE(ent, netvar.m_bFeignDeathReady))
                    AddEntityString(ent, ready_ringer_str, colors::FromRGBA8(178.0f, 0.0f, 255.0f, 255.0f));
                if (HasCondition<TFCond_Disguised>(ent))
                    AddEntityString(ent, disguised_str, colors::FromRGBA8(220, 220, 220, 255));
                if (HasCondition<TFCond_GasCoated>(ent))
                    AddEntityString(ent, gassed_str, colors::FromRGBA8(0, 128, 0, 255));
                // Uber/Bonk
                if (IsPlayerInvulnerable(ent))
                    AddEntityString(ent, invulnerable_str);
                // Vaccinator
                if (HasCondition<TFCond_UberBulletResist>(ent))
                {
                    AddEntityString(ent, bullet_a_str, colors::FromRGBA8(220, 220, 220, 255));
                }
                else if (HasCondition<TFCond_SmallBulletResist>(ent))
                {
                    AddEntityString(ent, bullet_p_str);
                }
                if (HasCondition<TFCond_UberFireResist>(ent))
                {
                    AddEntityString(ent, fire_a_str, colors::FromRGBA8(220, 220, 220, 255));
                }
                else if (HasCondition<TFCond_SmallFireResist>(ent))
                {
                    AddEntityString(ent, fire_p_str);
                }
                if (HasCondition<TFCond_UberBlastResist>(ent))
                {
                    AddEntityString(ent, blast_a_str, colors::FromRGBA8(220, 220, 220, 255));
                }
                else if (HasCondition<TFCond_SmallBlastResist>(ent))
                {
                    AddEntityString(ent, blast_p_str);
                }
                // Crit
                if (IsPlayerCritBoosted(ent))
                    AddEntityString(ent, crit_str, colors::orange);

                // We want revving, zoomed and slowed to be mutually exclusive. Otherwise slowed and zoomed/revving will show at the same time.
                // Revving
                auto weapon_idx      = HandleToIDX(CE_INT(ent, netvar.hActiveWeapon));
                CachedEntity *weapon = IDX_GOOD(weapon_idx) ? ENTITY(weapon_idx) : nullptr;
                if (CE_GOOD(weapon) && weapon->m_iClassID() == CL_CLASS(CTFMinigun) && CE_INT(weapon, netvar.iWeaponState) != 0)
                    AddEntityString(ent, revving_str, colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
                // Zoomed
                else if (HasCondition<TFCond_Zoomed>(ent))
                    AddEntityString(ent, zooming_str, colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
                // Slowed
                else if (HasCondition<TFCond_Slowed>(ent))
                    AddEntityString(ent, slowed_str, colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));

                // Jarated
                if (HasCondition<TFCond_Jarated>(ent))
                    AddEntityString(ent, jarated_str, colors::yellow);
                // Taunting
                if (HasCondition<TFCond_Taunting>(ent))
                    AddEntityString(ent, taunting_str, colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
                // Dormant
                if (CE_VALID(ent) && RAW_ENT(ent)->IsDormant())
                    AddEntityString(ent, dormant_str, colors::red);
            }

            // Active weapon esp
            if (show_weapon)
            {
                int widx = HandleToIDX(CE_INT(ent, netvar.hActiveWeapon));
                if (IDX_GOOD(widx))
                {
                    CachedEntity *weapon = ENTITY(widx);
                    if (CE_VALID(weapon) && re::C_BaseCombatWeapon::IsBaseCombatWeapon(RAW_ENT(weapon)))
                    {
                        const char *weapon_name = g_ILocalize->FindAsUTF8(re::C_BaseCombatWeapon::GetPrintName(RAW_ENT(weapon)));
                        if (weapon_name)
                            AddEntityString(ent, std::string(weapon_name));
                    }
                }
            }

            // Notify esp to repaint
            espdata.needs_paint = true;
        }
    }

    if (espdata.needs_paint)
    {
        // Set entity color
        rgba_t color = colors::EntityF(ent);
        if (RAW_ENT(ent)->IsDormant())
        {
            color.r *= 0.5f;
            color.g *= 0.5f;
            color.b *= 0.5f;
        }
        // If show distance, add string here
        if (show_distance)
            AddEntityString(ent, format(int(distance / 64 * 1.22f), 'm'));
        {
            // range check with a bit extra just to be safe
            if (distance < 310)
                AddEntityString(ent, "DANGER ZONE!", colors::FromRGBA8(255.0f, 0.0f, 0.0f, 255.0f));
            else
                AddEntityString(ent, "SAFE ZONE", colors::FromRGBA8(0.0f, 255.0f, 0.0f, 255.0f));
        }
        SetEntityColor(ent, color);
    }
}

// Draw 3D box around player/building
void _FASTCALL Draw3DBox(CachedEntity *ent, const rgba_t &clr)
{
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return;

    Vector origin = RAW_ENT(ent)->GetCollideable()->GetCollisionOrigin();
    Vector mins   = RAW_ENT(ent)->GetCollideable()->OBBMins();
    Vector maxs   = RAW_ENT(ent)->GetCollideable()->OBBMaxs();
    // Dormant
    if (RAW_ENT(ent)->IsDormant())
        origin = *ent->m_vecDormantOrigin();

    // Create a array for storing box points
    Vector corners[8]; // World vectors
    Vector points[8];  // Screen vectors

    // Create points for the box based on max and mins
    float x    = maxs.x - mins.x;
    float y    = maxs.y - mins.y;
    float z    = maxs.z - mins.z;
    corners[0] = mins;
    corners[1] = mins + Vector(x, 0, 0);
    corners[2] = mins + Vector(x, y, 0);
    corners[3] = mins + Vector(0, y, 0);
    corners[4] = mins + Vector(0, 0, z);
    corners[5] = mins + Vector(x, 0, z);
    corners[6] = mins + Vector(x, y, z);
    corners[7] = mins + Vector(0, y, z);

    // Rotate the box and check if any point of the box isnt on the screen
    for (int i = 0; i < 8; ++i)
    {
        float yaw    = NET_VECTOR(RAW_ENT(ent), netvar.m_angEyeAngles).y;
        float s      = sinf(DEG2RAD(yaw));
        float c      = cosf(DEG2RAD(yaw));
        float xx     = corners[i].x;
        float yy     = corners[i].y;
        corners[i].x = (xx * c) - (yy * s);
        corners[i].y = (xx * s) + (yy * c);
        corners[i] += origin;

        if (!draw::WorldToScreen(corners[i], points[i]))
            return;
    }
    rgba_t draw_clr = clr;
    // Draw the actual box
    for (int i = 1; i <= 4; ++i)
    {
        draw::Line((points[i - 1].x), (points[i - 1].y), (points[i % 4].x) - (points[i - 1].x), (points[i % 4].y) - (points[i - 1].y), draw_clr, 0.5f);
        draw::Line((points[i - 1].x), (points[i - 1].y), (points[i + 3].x) - (points[i - 1].x), (points[i + 3].y) - (points[i - 1].y), draw_clr, 0.5f);
        draw::Line((points[i + 3].x), (points[i + 3].y), (points[i % 4 + 4].x) - (points[i + 3].x), (points[i % 4 + 4].y) - (points[i + 3].y), draw_clr, 0.5f);
    }
}

void Shutdown()
{
    data.clear();
}

// Draw a box around a player
void _FASTCALL DrawBox(CachedEntity *ent, const rgba_t &clr)
{
    PROF_SECTION(PT_esp_drawbox);

    // Check if ent is bad to prevent crashes
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return;

    // Get our collidable bounds
    if (!GetCollide(ent))
        return;

    // Pull the cached collide info
    ESPData &ent_data = data[ent->m_IDX];
    int max_x         = ent_data.collide_max.x;
    int max_y         = ent_data.collide_max.y;
    int min_x         = ent_data.collide_min.x;
    int min_y         = ent_data.collide_min.y;

    // Depending on whether the player is cloaked, we change the color
    // acordingly
    rgba_t border = ((ent->m_Type() == ENTITY_PLAYER) && IsPlayerInvisible(ent)) ? colors::FromRGBA8(192, 192, 192, clr.a * 255.0f) : colors::Transparent(colors::black, clr.a);

    switch (*box_esp)
    {
    case 2: // With box corners, we draw differently
    {
        BoxCorners(min_x, min_y, max_x, max_y, clr, (clr.a != 1.0f));
        break;
    }
    default: // Otherwise, we just do simple draw funcs
    {
        draw::RectangleOutlined(min_x, min_y, max_x - min_x, max_y - min_y, border, 0.5f);
        draw::RectangleOutlined(min_x + 1, min_y + 1, max_x - min_x - 2, max_y - min_y - 2, clr, 0.5f);
        draw::RectangleOutlined(min_x + 2, min_y + 2, max_x - min_x - 4, max_y - min_y - 4, border, 0.5f);
        break;
    }
    }
}

// Function to draw box corners, Used by DrawBox
void BoxCorners(int minx, int miny, int maxx, int maxy, const rgba_t &color, bool transparent)
{
    const rgba_t &black    = transparent ? colors::Transparent(colors::black) : colors::black;
    const float heightSize = ((float) *box_corner_size_height / 100) * (maxy - miny - 3);
    const float widthSize  = ((float) *box_corner_size_width / 100) * (maxx - minx - 2);

    // Black corners
    // Top Left
    draw::Rectangle(minx, miny, widthSize + 1, 3, black);
    draw::Rectangle(minx, miny + 3, 3, heightSize - 3, black);
    // Top Right
    draw::Rectangle(maxx, miny, -widthSize - 1, 3, black);
    draw::Rectangle(maxx - 3 + 1, miny + 3, 3, heightSize - 3, black);
    // Bottom Left
    draw::Rectangle(minx, maxy - 3, widthSize + 1, 3, black);
    draw::Rectangle(minx, maxy, 3, -heightSize - 3, black);
    // Bottom Right
    draw::Rectangle(maxx, maxy - 3, -widthSize - 1, 3, black);
    draw::Rectangle(maxx - 2, maxy, 3, -heightSize - 3, black);

    // Colored corners
    // Top Left
    draw::Line(minx + 1, miny + 1, widthSize, 0, color, 0.5f);
    draw::Line(minx + 1, miny + 1, 0, heightSize, color, 0.5f);
    // Top Right
    draw::Line(maxx - 1, miny + 1, -widthSize, 0, color, 0.5f);
    draw::Line(maxx - 1, miny + 1, 0, heightSize, color, 0.5f);
    // Bottom Left
    draw::Line(minx + 1, maxy - 2, widthSize, 0, color, 0.5f);
    draw::Line(minx + 1, maxy - 2, 0, -heightSize, color, 0.5f);
    // Bottom Right
    draw::Line(maxx - 1, maxy - 2, -widthSize, 0, color, 0.5f);
    draw::Line(maxx - 1, maxy - 2, 0, -heightSize, color, 0.5f);
}

// Used for caching collidable bounds
bool GetCollide(CachedEntity *ent)
{
    PROF_SECTION(PT_esp_getcollide);

    // Null check to prevent crashing
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return false;

    // Grab esp data
    ESPData &ent_data = data[ent->m_IDX];

    // If entity has cached collides, return it. Otherwise generate new bounds
    if (!ent_data.has_collide)
    {

        // Get collision center, max, and mins
        Vector origin = RAW_ENT(ent)->GetCollideable()->GetCollisionOrigin();
        // Dormant
        if (RAW_ENT(ent)->IsDormant())
        {
            auto vec = ent->m_vecDormantOrigin();
            if (!vec)
                return false;
            origin = *vec;
        }
        Vector mins = RAW_ENT(ent)->GetCollideable()->OBBMins() + origin;
        Vector maxs = RAW_ENT(ent)->GetCollideable()->OBBMaxs() + origin;

        // Create a array for storing box points
        Vector points_r[8]; // World vectors
        Vector points[8];   // Screen vectors

        // If user setting for box expnad is true, spread the max and mins
        if (esp_expand)
        {
            const float exp = *esp_expand;
            maxs.x += exp;
            maxs.y += exp;
            maxs.z += exp;
            mins.x -= exp;
            mins.y -= exp;
            mins.z -= exp;
        }

        // Create points for the box based on max and mins
        float x     = maxs.x - mins.x;
        float y     = maxs.y - mins.y;
        float z     = maxs.z - mins.z;
        points_r[0] = mins;
        points_r[1] = mins + Vector(x, 0, 0);
        points_r[2] = mins + Vector(x, y, 0);
        points_r[3] = mins + Vector(0, y, 0);
        points_r[4] = mins + Vector(0, 0, z);
        points_r[5] = mins + Vector(x, 0, z);
        points_r[6] = mins + Vector(x, y, z);
        points_r[7] = mins + Vector(0, y, z);

        for (int i = 0; i < 8; ++i)
        {
            if (!draw::WorldToScreen(points_r[i], points[i]))
                return false;
        }

        // Get max and min of the box using the newly created screen vector
        int max_x = -1;
        int max_y = -1;
        int min_x = 65536;
        int min_y = 65536;
        for (int i = 0; i < 8; ++i)
        {
            if (points[i].x > max_x)
                max_x = points[i].x;
            if (points[i].y > max_y)
                max_y = points[i].y;
            if (points[i].x < min_x)
                min_x = points[i].x;
            if (points[i].y < min_y)
                min_y = points[i].y;
        }

        // Save the info to the esp data and notify cached that we cached info.
        ent_data.collide_max = Vector(max_x, max_y, 0);
        ent_data.collide_min = Vector(min_x, min_y, 0);
        ent_data.has_collide = true;

        return true;
    }
    else // We already have collidable so return true.
        return true;
    // Impossible error, return false
    return false;
}
// Function to reset entitys strings
void ResetEntityStrings(bool full_clear)
{
    if (full_clear)
        for (auto &[key, val] : data)
        {
            val.string_count = 0;
            val.color        = colors::empty;
            val.needs_paint  = false;
        }
    else
        for (u_int16_t i = 1; i < g_GlobalVars->maxClients; ++i)
        {
            auto &element        = data[i];
            element.string_count = 0;
            element.color        = colors::empty;
            element.needs_paint  = false;
        }
}

// Sets an entitys esp color
inline void SetEntityColor(CachedEntity *entity, const rgba_t &color)
{
    if (entity->m_IDX > 2047 || entity->m_IDX < 0)
        return;
    data[entity->m_IDX].color = color;
}

static InitRoutine init(
    []()
    {
        EC::Register(EC::CreateMove, cm, "cm_esp", EC::average);
#if ENABLE_VISUALS
        EC::Register(EC::Draw, Draw, "draw_esp", EC::average);
#endif
    });

} // namespace hacks::esp