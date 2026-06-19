#include "GatewayServer.h"
#include "GatewaySession.h"
#include "BufferManager.h"
#include "../extra/dubu_echo_packet_generated.h"

GatewayServer::GatewayServer() : DUBU::Server()
{
}

GatewayServer::~GatewayServer()
{
}

void GatewayServer::Initialize(const Map<Uint8, DUBU::Packet::PacketHandler>* handlers)
{
    DUBU::Server::Initialize(handlers);

    {
        // 에코 서버 세션 생성
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

        echoSessionList_.reserve(echoCount_);
        for (Int32 i = 0; i < echoCount_; ++i)
        {
            GatewaySession* session = CreateGatewaySession(echoServerAddr);
            echoSessionList_.emplace_back(session);
            session->SetAwaysConnect(true);
        }
    }
}

GatewaySession* GatewayServer::CreateGatewaySession(const SOCKADDR_IN& addr)
{
    // GatewaySession : 구체적으로 인게임 / 소셜에 대한 세션
    GatewaySession* session = GetSessionManager().AddSession<GatewaySession>();
    session->SetSockAddr(addr);
    session->SetSocket(rudpSocket_.get());
    session->SetTimestamp(DUBU::GetCurrentTimeMs());

    return session;
}

void GatewayServer::SendToEcho(Uint8* buffer, Uint8 code, Uint16 size)
{
    DUBU::OverlappedPacketBuffer* opb = reinterpret_cast<DUBU::OverlappedPacketBuffer*>(buffer);
    DUBU::Packet::PacketHeader* header = reinterpret_cast<DUBU::Packet::PacketHeader*>(opb->buffer_);

    // 나머지 연산으로 분산해서 각 세션 전송
    echoSessionList_[header->sessionId_ % echoCount_]->SendPacket(buffer, code, size);
}
