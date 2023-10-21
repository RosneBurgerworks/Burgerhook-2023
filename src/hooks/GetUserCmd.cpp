#include "HookedMethods.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(GetUserCmd, CUserCmd *, IInput *this_, int sequence_number)
{
    return &GetCmds(this_)[sequence_number % VERIFIED_CMD_SIZE];
}
} // namespace hooked_methods
