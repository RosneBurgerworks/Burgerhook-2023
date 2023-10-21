#pragma once

#include <menu/BaseMenuObject.hpp>
#include <menu/object/Text.hpp>

namespace zerokernel
{

class WMWindow;

class Task : public BaseMenuObject
{
public:
    explicit Task(WMWindow &window);

    //

    ~Task() override = default;

    void render() override;

    bool onLeftMouseClick() override;

    void recalculateSize() override;

    void onMove() override;

    void recursiveSizeUpdate() override;

    void emitSizeUpdate() override;

    //

    WMWindow &window;
    Text text{};
};
} // namespace zerokernel