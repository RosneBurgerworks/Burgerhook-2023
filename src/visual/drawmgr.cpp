#include <MiscTemporary.hpp>
#include <hacks/hacklist.hpp>
#if ENABLE_IMGUI_DRAWING
#include "imgui/imrenderer.hpp"
#endif
#include <settings/Bool.hpp>
#include <menu/GuiInterface.hpp>
#include "common.hpp"
#include "visual/drawing.hpp"
#include "hack.hpp"
#include "drawmgr.hpp"

static settings::Boolean info_text{"hack-info.enable", "true"};

void render_cheat_visuals()
{
    {
        PROF_SECTION(BeginCheatVisuals)
        BeginCheatVisuals();
    }
    {
        PROF_SECTION(DrawCheatVisuals)
        DrawCheatVisuals();
    }
    {
        PROF_SECTION(EndCheatVisuals)
        EndCheatVisuals();
    }
}
int currentBuffer = 0;

void BeginCheatVisuals()
{
#if ENABLE_IMGUI_DRAWING
    im_renderer::bufferBegin();
#endif
    ResetStrings();
}

void DrawCheatVisuals()
{
    {
        PROF_SECTION(DRAW_info)
        std::string name_s, reason_s;
        if (info_text && draw::inited)
        {
            auto color = colors::FromRGBA8(255, 255, 0, 255);
            color.a    = 1.0f;
            AddSideString("burgerhook indev!!!!", color);
        }
    }
    if (spectator_target)
        AddCenterString("Press SPACE to stop spectating");
    {
        PROF_SECTION(DRAW_WRAPPER)
        EC::run(EC::Draw);
    }
    if (CE_GOOD(g_pLocalPlayer->entity))
    {
        Prediction_PaintTraverse();
    }
    {
        PROF_SECTION(DRAW_strings)
        DrawStrings();
    }
#if ENABLE_GUI
    {
        PROF_SECTION(DRAW_GUI)
        gui::draw();
    }
#endif
}

void EndCheatVisuals()
{
#if !ENABLE_ENGINE_DRAWING && !ENABLE_IMGUI_DRAWING
    buffers[currentBuffer]->end();
#endif
#if !ENABLE_ENGINE_DRAWING || ENABLE_IMGUI_DRAWING
    currentBuffer = !currentBuffer;
#endif
}

void DrawCache()
{
#if !ENABLE_ENGINE_DRAWING && !ENABLE_IMGUI_DRAWING
    buffers[!currentBuffer]->replay();
#endif
}
