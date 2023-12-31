#pragma once

#include <settings/Settings.hpp>
#include <menu/object/container/Container.hpp>

namespace zerokernel
{

class ColorSelector : public BaseMenuObject
{
public:
    ~ColorSelector() override = default;

    ColorSelector();
    explicit ColorSelector(settings::Variable<rgba_t> &variable);

    void render() override;

    bool onLeftMouseClick() override;

    void loadFromXml(const tinyxml2::XMLElement *data) override;

protected:
    settings::Variable<rgba_t> *variable{ nullptr };
};
} // namespace zerokernel