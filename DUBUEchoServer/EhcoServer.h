#pragma once
#include "Server.h"

class EhcoServer : public DUBU::Server
{
public:
    EhcoServer();
    virtual ~EhcoServer();

    void Broadcast();
private:

};

