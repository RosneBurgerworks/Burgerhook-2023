#include <menu/BaseMenuObject.hpp>
#include <menu/wm/WindowCloseButton.hpp>

#include <menu/wm/WMWindow.hpp>
#include "pathio.hpp"
#include "drawing.hpp"

namespace zerokernel_windowclosebutton
{
static settings::RVariable<rgba_t> background_hover{ "zk.style.window-close-button.color.background-hover", "ff0000" };
static settings::RVariable<rgba_t> color_border{ "zk.style.window-close-button.color.border", "000000ff" };
} // namespace zerokernel_windowclosebutton

void zerokernel::WindowCloseButton::render()
{

}

zerokernel::WindowCloseButton::WindowCloseButton() : BaseMenuObject{}
{
    bb.resize(16, 16);
}

bool zerokernel::WindowCloseButton::onLeftMouseClick()
{

}
