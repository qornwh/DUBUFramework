#pragma once
#include "Singleton.h"

namespace DUBU
{
    class RUDPSocket;
    class Session;
}

class EchoClient;

class EchoClientHander : public DUBU::Singleton<EchoClientHander>
{
public:
    EchoClientHander();
    ~EchoClientHander();

    // chat
    Bool ChatVerifier(flatbuffers::Verifier& verifier);
    void ChatHandler(DUBU::Session* session, Uint8* buffer, Int32 size);

    // bot
    Bool BotVerifier(flatbuffers::Verifier& verifier);
    void BotHandler(DUBU::Session* session, Uint8* buffer, Int32 size);

    void SetOwner(EchoClient* owner);
private:

    // EchoClient 참조
    EchoClient* owner_;
};
