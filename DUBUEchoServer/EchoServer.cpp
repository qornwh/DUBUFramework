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

        DUBU::Packet::PacketOpctions opt{ true, true, haeder->packetCode_ };
        if (size + sh.GetSize() + sizeof(DUBU::Packet::PacketHeader) > PACKET_MAX_SIZE)
        {
            // 너무 길면 순서는 버린다.
            opt.order_ = false;
        }
        session->SendPacket(buffer, code, size, opt, reinterpret_cast<Uint8*>(&sh), sh.GetSize());
    }
}
