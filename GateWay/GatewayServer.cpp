#include "GatewayServer.h"
#include "GatewaySession.h"
#include "BufferManager.h"
#include "InternalClient.h"
#include "../extra/dubu_echo_packet_generated.h"
#include <Subheader.h>
#include "../extra/GatewaySubHeader.h"

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

void GatewayServer::SendToResgister(Uint8* buffer, Uint8 code, Uint16 size)
{
    DUBU::Packet::PacketHeader* header = reinterpret_cast<DUBU::Packet::PacketHeader*>(buffer);
    DUBU::Packet::PacketOpctions opt{ true, true, 0 };
    // 여기서는 어짜피 헤더 정보만 쓰고, 서브헤더는 쓰일일 없다.

    flatbuffers::FlatBufferBuilder fbb;
    // 일단 클라이언트의 세션 id를 키로 가정함.
    auto regist = DUBU::Echo::CreateRegister(fbb, header->sessionId_);
    auto packet = DUBU::Echo::CreatePacket(fbb, DUBU::Echo::PacketBody_Register, regist.Union());
    DUBU::Echo::FinishPacketBuffer(fbb, packet);

    Uint8* newBuffer = fbb.GetBufferPointer();
    Uint16 newSize = static_cast<Uint16>(fbb.GetSize());

    internalClientList_[header->sessionId_ % echoCount_]->SendPacket(newBuffer, code, newSize, opt);
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

void GatewayServer::SendToClient(Uint32 sessionId, Uint8* buffer, Uint8 code, Uint16 size)
{
    DUBU::ReadLockGuard rl(GetSessionLock());
    DUBU::Session* session = GetSessionManager().GetSession(sessionId);
    if (session == nullptr)
    {
        // 이미 끊긴 클라 - 드랍
        return;
    }

    DUBU::Packet::PacketOpctions opt{ true, true, 0 };
    session->SendPacket(buffer, code, size, opt);
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

Uint64 GatewayServer::GatewayKey(const SOCKADDR_IN& addr)
{
    return ((Uint64)addr.sin_addr.s_addr << 16) | (Uint64)ntohs(addr.sin_port);
}
