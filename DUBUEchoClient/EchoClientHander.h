#pragma once
#include "Singleton.h"

namespace DUBU
{
    class RUDPSocket;
}

class EchoClient;

class EchoClientHander : public DUBU::Singleton<EchoClientHander>
{
public:
    EchoClientHander();
    ~EchoClientHander();

    // chat
    Bool ChatVerifier(flatbuffers::Verifier& verifier);
    void ChatHandler(Uint8* buffer, Int32 size);

    void SetOwner(EchoClient* owner);
private:

    // EchoClient 참조
    EchoClient* owner_; 
};

