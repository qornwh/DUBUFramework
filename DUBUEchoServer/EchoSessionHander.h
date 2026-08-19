#pragma once
#include "Singleton.h"

namespace DUBU
{
    class RUDPSocket;
    class Session;
}

class EchoServer;

class EchoSessionHander : public DUBU::Singleton<EchoSessionHander>
{
public:
    EchoSessionHander();
    ~EchoSessionHander();

    // register
    Bool RegisterVerifier(flatbuffers::Verifier& verifier);
    void RegisterHandler(DUBU::Session* session, Uint8* buffer, Int32 size);

    // chat
    Bool ChatVerifier(flatbuffers::Verifier& verifier);
    void ChatHandler(DUBU::Session* session, Uint8* buffer, Int32 size);

    // bot
    Bool BotVerifier(flatbuffers::Verifier& verifier);
    void BotHandler(DUBU::Session* session, Uint8* buffer, Int32 size);

    void SetOwner(EchoServer* owner);
private:
    // EchoServer 참조
    EchoServer* owner_;
};
