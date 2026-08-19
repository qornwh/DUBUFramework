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

    // echo 등록
    Bool RegisterVerifier(flatbuffers::Verifier& verifier);
    void RegisterHandler(DUBU::Session* session, Uint8* buffer, Int32 size);
    void RegisterHandler2(DUBU::Session* session, Uint8* buffer, Int32 size, Uint8* subBuf, Uint8 type);

    // echo 테스트
    Bool ChatVerifier(flatbuffers::Verifier& verifier);
    void ChatHandler(DUBU::Session* session, Uint8* buffer, Int32 size);
    void ChatHandler2(DUBU::Session* session, Uint8* buffer, Int32 size, Uint8* subBuf, Uint8 type);

    void SetOwner(GatewayServer* owner);

    void SetClientToGatway(Bool isClientToGatway);
private:
    // 보낼서버 참조
    GatewayServer* owner_;

    // 클라 - 게이트 웨이 판단
    Bool clientToGatway_ = false;
};

