#pragma once
#include "Server.h"
#include "Session.h"

class EchoServer : public DUBU::Server
{
public:
    EchoServer();
    virtual ~EchoServer();

    void Broadcast(Uint8* buffer, Uint8 code, Uint16 size, Uint8 channelId);

    // 등록 메시지 수신시 클라 등록
    void AddConnection(Uint32 clientId, DUBU::Session* session);
    // 내부 커넥션 종료시 등록 정리
    virtual void DestroySession(DUBU::Session* session) override;
private:
    Uint64 GatewayKey(const SOCKADDR_IN& addr);
    // 클라 키, 그 클라 등록이 수신된 세션
    Map<Uint32, DUBU::Session*> connectionList_;
    DUBU::Lock connectionLock_;
};

