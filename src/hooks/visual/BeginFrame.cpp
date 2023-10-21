#include "HookedMethods.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(BeginFrame, void, IStudioRender *this_)
{
    return original::BeginFrame(this_);
}
} // namespace hooked_methods