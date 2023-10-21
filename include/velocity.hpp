#pragma once

#include <functional>
class IClientEntity;
class Vector;

namespace velocity
{
typedef std::function<void(IClientEntity *, Vector &)> EstimateAbsVelocity_t;
extern EstimateAbsVelocity_t EstimateAbsVelocity;

void Init();
} // namespace velocity
