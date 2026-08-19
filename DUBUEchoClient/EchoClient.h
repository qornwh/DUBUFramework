#pragma once
#include "Client.h"

/*
 * EchoClient : 채팅 테스트용 클라이언트
 */
class EchoClient : public DUBU::Client
{
public:
    EchoClient(const String& serverIP, Uint16 serverPort, const Map<Uint8, DUBU::Packet::PacketHandler>* handlers = nullptr);
    virtual ~EchoClient();

    // 접속 후 1회, 게이트웨이에 등록 요청
    void SendRegisterMessage();
    void SendChatMessage(const String& chat, Uint8 channelId = 1);
    void SendDumpChatMessage(const String& chat, Uint8 channelId = 0);
private:

};

