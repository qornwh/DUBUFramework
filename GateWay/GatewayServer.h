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

    // 에코 서버로 전송
    void SendToResgister(Uint8* buffer, Uint8 code, Uint16 size);

    // 에코 서버로 전송
    void SendToEcho(Uint8* buffer, Uint8 code, Uint16 size);

    // 에코 회신을 해당 클라 세션으로 단일 전송
    void SendToClient(Uint32 sessionId, Uint8* buffer, Uint8 code, Uint16 size, const DUBU::Packet::PacketOpctions& opt);

    // 봇 중계 전용
    void SendBotToEcho(Uint8* buffer, Uint16 size);

    // 내부 서버 dispatch
    Bool InnerDispatch();

    void Broadcast(Uint8* buffer, Uint8 code, Uint16 size);


private:
    // 임시로 일단 10개 테스트
    const Uint64 echoCount_ = 10;
    Vector<DUBU::InternalClient*> internalClientList_;
};

