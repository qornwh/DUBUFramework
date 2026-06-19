#pragma once
#include "pch.h"
#include "Packet.h"
#include "Server.h"

namespace DUBU
{
    namespace Echo
    {
        struct Packet;
    }
    class InternalClient;
}
class GatewaySession;

class GatewayServer : public DUBU::Server
{
public:
    GatewayServer();
    virtual ~GatewayServer();

    void Initialize(const Map<Uint8, DUBU::Packet::PacketHandler>* handlers) override;

    GatewaySession* CreateGatewaySession(const SOCKADDR_IN& addr);

    // 에코 서버로 전송
    void SendToEcho(Uint8* buffer, Uint8 code, Uint16 size);

private:
    // 임시로 일단 1개 테스트
    const Uint64 echoCount_ = 1;
    Vector<DUBU::InternalClient*> internalClientList_;
};

