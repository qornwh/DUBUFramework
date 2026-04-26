#pragma once
#include "Server.h"

class EchoServer : public DUBU::Server
{
public:
    EchoServer();
    virtual ~EchoServer();

    void Broadcast(Uint8* buffer, Uint8 code, Uint16 size);
private:

};

