#pragma once
#include "pch.h"
#include "Session.h"

class GatewaySession : public DUBU::Session
{
public:
    GatewaySession(const Map<Uint8, DUBU::Packet::PacketHandler>* handlers);
    virtual ~GatewaySession();

private:
};

