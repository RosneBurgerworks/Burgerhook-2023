#pragma once

#include "string"
class bf_read;

namespace votelogger
{

void dispatchUserMessage(bf_read &buffer, int type);
void onShutdown(std::string message);
} // namespace votelogger
