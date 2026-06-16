#include "GatewayServer.h"
#include "GatewaySession.h"

GatewayServer::GatewayServer(int sessionCount = 1) : DUBU::Server(), sessionCount_(sessionCount)
{
}

GatewayServer::~GatewayServer()
{
}

DUBU::Session* GatewayServer::CreateSession(const SOCKADDR_IN& addr)
{
    GatewaySession* session = GetSessionManager().AddSession<GatewaySession>();
    session->SetSockAddr(addr);
    session->SetSocket(rudpSocket_.get());
    session->SetTimestamp(DUBU::GetCurrentTimeMs());
#ifdef _DEBUG
    newSessionCount_.fetch_add(1);
#endif // _DEBUG

    return session;
}