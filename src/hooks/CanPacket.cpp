#include "HookedMethods.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(CanPacket, bool, INetChannel *this_)
{
    return original::CanPacket(this_);
}
} // namespace hooked_methods
