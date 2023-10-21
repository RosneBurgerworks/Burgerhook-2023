#pragma once

#include <settings/Settings.hpp>
#include <menu/Menu.hpp>

namespace zerokernel
{

class InputKey : public BaseMenuObject
{
public:
    ~InputKey() override = default;

    InputKey();

    explicit InputKey(settings::Variable<settings::Key> &key);

    bool handleSdlEvent(SDL_Event *event) override;

    void render() override;

    void onMove() override;

    void loadFromXml(const tinyxml2::XMLElement *data) override;

    Text text{};
    settings::Variable<settings::Key> *key{};
    bool capturing{ false };
};
} // namespace zerokernel
