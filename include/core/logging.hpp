#pragma once

#include <stdio.h>
#include <fstream>

namespace logging
{
extern std::ofstream handle;

void Initialize();
void Shutdown();
void Info(const char *fmt, ...);
void File(const char *fmt, ...);
} // namespace logging
