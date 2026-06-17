#pragma once
#include "pch.h"
#include "Packet.h"
#include "Server.h"

namespace DUBU
{
    namespace Echo
    {
        class Packet;
    }
}
class DUBU::Session;

class GatewayServer : public DUBU::Server
{
public:
    GatewayServer();
    virtual ~GatewayServer();

    void Initialize(const Map<Uint8, DUBU::Packet::PacketHandler>* handlers) override;

    virtual DUBU::Session* CreateSession(const SOCKADDR_IN& addr);

    // 에코 서버로 전송
    void SendToEcho(Uint8* ptr, Uint8 code, Uint16 size);

private:
    // 다른 서버에 통신할 무언가 개발
    // 에코서버와 연결 개수
    const Uint64 echoCount_ = 1;
    Vector<DUBU::Session*> echoSessionList_;
};

