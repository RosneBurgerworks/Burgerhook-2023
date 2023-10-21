#pragma once

#include <mutex>

extern std::mutex drawing_mutex;

void render_cheat_visuals();

void BeginCheatVisuals();
void DrawCheatVisuals();
void EndCheatVisuals();
void DrawCache();
