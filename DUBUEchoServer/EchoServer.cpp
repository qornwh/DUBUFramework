#include "EchoServer.h"
#include "RWLock.h"

EchoServer::EchoServer() : DUBU::Server()
{
}

EchoServer::~EchoServer()
{
}

void EchoServer::Broadcast(Uint8* buffer, Uint8 code, Uint16 size)
{
    DUBU::ReadLockGuard rl(GetSessionLock());

    for (auto [_, session] : GetSessionManager().GetSessions())
    {
        if (session == nullptr)
        {
            continue;
        }

        session->SendPacketReliable(buffer, code, size);
    }
}
