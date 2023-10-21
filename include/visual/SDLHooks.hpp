#pragma once

struct SDL_Window;

namespace sdl_hooks
{

extern SDL_Window *window;

void applySdlHooks();
void cleanSdlHooks();
} // namespace sdl_hooks