#include <settings/Bool.hpp>
#include "HookedMethods.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(RandomInt, int, IUniformRandomStream *this_, int min, int max)
{
    return original::RandomInt(this_, min, max);
}
} // namespace hooked_methods