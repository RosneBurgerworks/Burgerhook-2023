#include "HookedMethods.hpp"

namespace hacks::killstreak
{
extern void fire_event(IGameEvent *event);
}
namespace hooked_methods
{
// Unused right now
// TODO: maybe remove?
DEFINE_HOOKED_METHOD(FireEvent, bool, IGameEventManager2 *this_, IGameEvent *event, bool no_broadcast)
{
    // hacks::killstreak::fire_event(event);

    return original::FireEvent(this_, event, no_broadcast);
}
} // namespace hooked_methods