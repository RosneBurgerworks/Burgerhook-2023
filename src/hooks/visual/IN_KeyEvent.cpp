#include "HookedMethods.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(IN_KeyEvent, int, void *this_, int eventcode, ButtonCode_t keynum, const char *binding)
{
    return original::IN_KeyEvent(this_, eventcode, keynum, binding);
}
} // namespace hooked_methods
