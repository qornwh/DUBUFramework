#include "EchoServer.h"
#include "RWLock.h"
#include "../extra/GatewaySubHeader.h"

EchoServer::EchoServer() : DUBU::Server()
{
}

EchoServer::~EchoServer()
{
}

void EchoServer::AddConnection(Uint32 clientId, DUBU::Session* session)
{
    DUBU::WriteLockGuard wl(connectionLock_);
    connectionList_[clientId] = session;
}

void EchoServer::DestroySession(DUBU::Session* session)
{
    {
        // 죽는 세션을 참조하는 등록 제거
        DUBU::WriteLockGuard wl(connectionLock_);
        for (auto it = connectionList_.begin(); it != connectionList_.end();)
        {
            if (it->second == session)
            {
                it = connectionList_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    DUBU::Server::DestroySession(session);
}

void EchoServer::Broadcast(Uint8* buffer, Uint8 code, Uint16 size, Uint8 channelId)
{
    DUBU::ReadLockGuard rl(connectionLock_);

    for (auto& [clientId, session] : connectionList_)
    {
        if (session == nullptr)
        {
            continue;
        }

        // 서브헤더 확인 코드(스택에 두고 send시 복사함)
        GatewaySubHeader sh
        {
            // 보낸 클라이언트의 세션 id
            clientId,
            // 지금 세션 id
            session->GetSessionId()
        };

        DUBU::Packet::PacketOpctions opt{ true, true, channelId };
        if (size + sh.GetSize() + sizeof(DUBU::Packet::PacketHeader) > PACKET_MAX_SIZE)
        {
            // 너무 길면 순서는 버린다.
            opt.order_ = false;
        }
        session->SendPacket(buffer, code, size, opt, reinterpret_cast<Uint8*>(&sh), sh.GetSize());
    }
}

Uint64 EchoServer::GatewayKey(const SOCKADDR_IN& addr)
{
    return ((Uint64)addr.sin_addr.s_addr << 16) | (Uint64)ntohs(addr.sin_port);
}
