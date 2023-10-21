#pragma once

#include <memory>
#include <menu/BaseMenuObject.hpp>

namespace zerokernel
{

class ObjectFactory
{
public:
    static std::unique_ptr<BaseMenuObject> createObjectFromXml(const tinyxml2::XMLElement *element);
    static std::unique_ptr<BaseMenuObject> createAutoVariable(const tinyxml2::XMLElement *element);
    static std::unique_ptr<BaseMenuObject> createFromPrefab(const std::string &prefab_id);
};
} // namespace zerokernel