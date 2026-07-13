#include "EchoServer.h"
#include "RWLock.h"
#include "../extra/GatewaySubHeader.h"

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

        DUBU::Packet::PacketHeader* haeder = reinterpret_cast<DUBU::Packet::PacketHeader*>(buffer);

        // 서브헤더 확인 코드(스택에 두고 send시 복사함)
        GatewaySubHeader sh
        {
            // 클라이언트의 id
            haeder->sessionId_,
            // 지금 세션 id
            session->GetSessionId()
        };

        session->SendPacket(buffer, code, size, DUBU::Packet::PacketOpctions{true, true, haeder->packetCode_}, reinterpret_cast<Uint8*>(&sh), sh.GetSize());
    }
}
