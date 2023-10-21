#include <SDL2/SDL_events.h>
#include <menu/BaseMenuObject.hpp>
#include <menu/tinyxml2.hpp>
#include <menu/object/container/ModalContainer.hpp>
#include <menu/ModalBehavior.hpp>
#include <menu/Menu.hpp>

namespace zerokernel_modalcontainer
{
static settings::RVariable<rgba_t> color_border{ "zk.style.modal-container.color.border", "000000ff" };
static settings::RVariable<rgba_t> color_background{ "zk.style.modal-container.color.background", "171717ff" };
} // namespace zerokernel_modalcontainer

bool zerokernel::ModalContainer::handleSdlEvent(SDL_Event *event)
{
    if (modal.shouldCloseOnEvent(event))
        modal.close();

    return Container::handleSdlEvent(event);
}

void zerokernel::ModalContainer::render()
{
    renderBackground(*zerokernel_modalcontainer::color_background);
    renderBorder(*zerokernel_modalcontainer::color_border);

    Container::render();
}

zerokernel::ModalContainer::ModalContainer() : modal(this)
{
}
