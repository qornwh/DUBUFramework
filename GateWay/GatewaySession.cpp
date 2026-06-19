#include "GatewaySession.h"

GatewaySession::GatewaySession(const Map<Uint8, DUBU::Packet::PacketHandler>* handlers) : DUBU::Session(handlers)
{
}

GatewaySession::~GatewaySession()
{
}
