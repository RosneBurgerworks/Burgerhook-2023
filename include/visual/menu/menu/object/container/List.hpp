#pragma once

#include <menu/object/container/Container.hpp>

namespace zerokernel
{

class List : public virtual Container
{
public:
    ~List() override = default;

    List();

    void reorderElements() override;

    // Properties

    int interval{ 3 };
};
} // namespace zerokernel