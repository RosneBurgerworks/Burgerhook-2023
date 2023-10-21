#pragma once

namespace zerokernel
{

class Message;

class IMessageHandler
{
public:
    virtual void handleMessage(Message &msg, bool is_relayed) = 0;
};
} // namespace zerokernel