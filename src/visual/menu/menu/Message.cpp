#include <menu/Message.hpp>

namespace zerokernel
{

Message::Message(std::string name) : name(name), kv(std::move(name))
{
}
} // namespace zerokernel