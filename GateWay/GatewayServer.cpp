#include "GatewayServer.h"
#include "GatewaySession.h"
#include "BufferManager.h"
#include "InternalClient.h"
#include "../extra/dubu_echo_packet_generated.h"
#include <Subheader.h>

GatewayServer::GatewayServer() : DUBU::Server()
{
}

GatewayServer::~GatewayServer()
{
    // 클라이언트 disconnect 소멸
    for (auto* client : internalClientList_)
    {
        if (client == nullptr)
        {
            continue;
        }

        if (client->IsConnect())
        {
            client->Disconnect();
        }

        DUBU::Push<DUBU::InternalClient>(client);
    }

    internalClientList_.clear();
}

void GatewayServer::Initialize(const Map<Uint8, DUBU::Packet::PacketHandler>* handlers)
{
    DUBU::Server::Initialize(handlers);

    {
        // internalClient 생성
        const String echoServerIp = "127.0.0.1";
        const Uint32 echoServerPort = 12345;
        SOCKADDR_IN echoServerAddr;
        memset(&echoServerAddr, 0, sizeof(echoServerAddr));
        echoServerAddr.sin_family = AF_INET;
        echoServerAddr.sin_port = htons(echoServerPort);
        if (inet_pton(AF_INET, echoServerIp.c_str(), &echoServerAddr.sin_addr) <= 0)
        {
            assert(false);
        }

        internalClientList_.reserve(echoCount_);
        for (Int32 i = 0; i < echoCount_; ++i)
        {
            DUBU::InternalClient* client = DUBU::Pop<DUBU::InternalClient>(echoServerIp, echoServerPort, handlers);
            internalClientList_.emplace_back(client);
        }

        for (Int32 i = 0; i < echoCount_; ++i)
        {
            internalClientList_[i]->ConnectTimes(5);
        }
    }
}

void GatewayServer::SendToEcho(Uint8* buffer, Uint8 code, Uint16 size)
{
    DUBU::Packet::PacketHeader* header = reinterpret_cast<DUBU::Packet::PacketHeader*>(buffer);
    DUBU::Packet::PacketOpctions opt{ true, true, 0 };

    // 헤더 길이 제외한 버퍼 크기를 구함
    Uint16 offset = sizeof(DUBU::Packet::PacketHeader);
    Uint8 shType = header->packetCode_ >> 5;
    if (shType > 0)
    {
        DUBU::Packet::SubheaderBase* sh = reinterpret_cast<DUBU::Packet::SubheaderBase*>(buffer + sizeof(DUBU::Packet::PacketHeader));
        offset += sh->GetSize();
    }

    internalClientList_[header->sessionId_ % echoCount_]->SendPacket(buffer + offset, code, size - offset, opt);
}

Bool GatewayServer::InnerDispatch()
{
    for (Int32 i = 0; i < echoCount_; ++i)
    {
        internalClientList_[i]->Dispatch();
    }

    return true;
}

void GatewayServer::Broadcast(Uint8* buffer, Uint8 code, Uint16 size)
{
    DUBU::ReadLockGuard rl(GetSessionLock());

    for (auto [_, session] : GetSessionManager().GetSessions())
    {
        if (session == nullptr)
        {
            continue;
        }

        DUBU::Packet::PacketOpctions opt{true, true, 0};
        session->SendPacket(buffer, code, size, opt);
    }
}
