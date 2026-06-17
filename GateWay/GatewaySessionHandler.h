#pragma once
#include "Singleton.h"

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
private:
    // 보낼서버 참조
    GatewayServer* owner_;
};

