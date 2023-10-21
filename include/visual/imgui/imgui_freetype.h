#pragma once

#include "imgui.h" // IMGUI_API, ImFontAtlas

namespace ImGuiFreeType
{
enum RasterizerFlags
{
    // By default, hinting is enabled and the font's native hinter is preferred over the auto-hinter.
    NoHinting     = 1 << 0, // Disable hinting. This generally generates 'blurrier' bitmap glyphs when the glyph are rendered in any of the anti-aliased modes.
    NoAutoHint    = 1 << 1, // Disable auto-hinter.
    ForceAutoHint = 1 << 2, // Indicates that the auto-hinter is preferred over the font's native hinter.
    LightHinting  = 1 << 3, // A lighter hinting algorithm for gray-level modes. Many generated glyphs are fuzzier but better resemble their original shape. This is achieved by snapping glyphs to the
                            // pixel grid only vertically (Y-axis), as is done by Microsoft's ClearType and Adobe's proprietary font renderer. This preserves inter-glyph spacing in horizontal text.
    MonoHinting = 1 << 4,   // Strong hinting algorithm that should only be used for monochrome output.
    Bold        = 1 << 5,   // Styling: Should we artificially embolden the font?
    Oblique     = 1 << 6    // Styling: Should we slant the font, emulating italic style?
};

IMGUI_API bool BuildFontAtlas(ImFontAtlas *atlas, unsigned int extra_flags = 0);

// By default ImGuiFreeType will use ImGui::MemAlloc()/MemFree().
// However, as FreeType does lots of allocations we provide a way for the user to redirect it to a separate memory heap if desired:
IMGUI_API void SetAllocatorFunctions(void *(*alloc_func)(size_t sz, void *user_data), void (*free_func)(void *ptr, void *user_data), void *user_data = NULL);
} // namespace ImGuiFreeType