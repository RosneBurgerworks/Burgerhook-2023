#pragma once

#include <menu/BaseMenuObject.hpp>
#include <menu/object/container/Container.hpp>

namespace zerokernel
{

class WindowManager;
class WMWindow;

class TaskBar : public Container
{
public:
    ~TaskBar() override = default;

    explicit TaskBar(WindowManager &wm);

    void reorderElements() override;

    bool isHidden() override;

    void render() override;

    //

    void addWindowButton(WMWindow &window);

    //

    WindowManager &wm;
};
} // namespace zerokernel