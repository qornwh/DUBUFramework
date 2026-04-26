#pragma once
#include "Singleton.h"

namespace DUBU
{
    class RUDPSocket;
}

class EchoServer;

class EchoSessionHander : public DUBU::Singleton<EchoSessionHander>
{
public:
    EchoSessionHander();
    ~EchoSessionHander();

    // chat
    Bool ChatVerifier(flatbuffers::Verifier& verifier);
    void ChatHandler(Uint8* buffer, Int32 size);

    void SetOwner(EchoServer* owner);
private:
    // EchoServer 참조
    EchoServer* owner_;
};
