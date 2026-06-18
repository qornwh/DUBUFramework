#pragma once
#include "Singleton.h"

namespace DUBU
{
    class Session;
}

class GatewayServer;

class GatewaySessionHandler : public DUBU::Singleton<GatewaySessionHandler>
{
public:
    GatewaySessionHandler();
    ~GatewaySessionHandler();

    // echo 테스트
    Bool ChatVerifier(flatbuffers::Verifier& verifier);
    void ChatHandler(Uint8* buffer, Int32 size);

    void SetOwner(GatewayServer* owner);
    void SetSession(DUBU::Session* session);

    void SetClientToGatway(Bool isClientToGatway);
private:
    // 보낼서버 참조
    GatewayServer* owner_;

    // 현재 세션
    DUBU::Session* session_;

    // 클라 - 게이트 웨이 판단
    Bool clientToGatway_ = false;
};

