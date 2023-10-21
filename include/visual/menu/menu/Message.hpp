#pragma once

#include <string>
#include <menu/BaseMenuObject.hpp>
#include <menu/KeyValue.hpp>

namespace zerokernel
{

class Message
{
public:
    explicit Message(std::string name);

    const std::string name;
    KeyValue kv;
    BaseMenuObject *sender{ nullptr };
};
} // namespace zerokernel