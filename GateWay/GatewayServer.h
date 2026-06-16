#pragma once
#include "Server.h"

class DUBU::Session;

class GatewayServer : public DUBU::Server
{
public:
    GatewayServer(int sessionCount);
    virtual ~GatewayServer();

    virtual DUBU::Session* CreateSession(const SOCKADDR_IN& addr);

private:
    int sessionCount_ = 1;
};

