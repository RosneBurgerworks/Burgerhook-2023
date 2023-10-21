#pragma once

#include <string>

namespace zerokernel
{

class StaticTextComponent
{
public:
    StaticTextComponent(const std::string &string, fonts::font &font);

    int x{};
    int y{};
    int padding_x{};
    int padding_y{};
    int size_x{};
    int size_y{};

    const std::string string;
    fonts::font &font;
};
} // namespace zerokernel
