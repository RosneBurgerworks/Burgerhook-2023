#pragma once

#include <menu/object/container/Container.hpp>

namespace zerokernel
{

class SmartContentContainer : public Container
{
public:
    enum class Direction
    {
        HORIZONTAL,
        VERTICAL
    };
    enum class JustifyContent
    {
        START,
        END,
        CENTER
    };

public:
    ~SmartContentContainer() override = default;

    SmartContentContainer() : Container()
    {
    }

    void notifySize() override
    {
        Container::notifySize();
    }

    // Specific methods

    void reorderObjects()
    {
    }

    // Properties

    Direction direction{ Direction::HORIZONTAL };
};
} // namespace zerokernel