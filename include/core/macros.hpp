#pragma once

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

constexpr int c_strcmp(char const *lhs, char const *rhs)
{
    return (('\0' == lhs[0]) && ('\0' == rhs[0])) ? 0 : (lhs[0] != rhs[0]) ? (lhs[0] - rhs[0]) : c_strcmp(lhs + 1, rhs + 1);
}