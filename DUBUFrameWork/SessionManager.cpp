#include "SessionManager.h"

DUBU::SessionManager::SessionManager()
{
}

DUBU::SessionManager::~SessionManager()
{    
    sessionMap_.clear();
}

DUBU::Session* DUBU::SessionManager::AddSession()
{
    Uint32 sessionId = GenerateId();
    sessionCount_.fetch_add(1);
    Session* session = Pop<Session>(handlers_);
    session->SetSessionId(sessionId);
    sessionMap_.insert({ sessionId, session });

    return session;
}

void DUBU::SessionManager::RemoveSession(Uint32 sessionId)
{
    sessionMap_.erase(sessionId);
}

DUBU::Session* DUBU::SessionManager::GetSession(Uint32 sessionId)
{
    auto it = sessionMap_.find(sessionId);
    if (it != sessionMap_.end())
    {
        return it->second;
    }
    return nullptr;
}

Map<Uint32, DUBU::Session*>& DUBU::SessionManager::GetSessions()
{
    return sessionMap_;
}

void DUBU::SessionManager::SetHandlers(const Map<Uint8, Packet::PacketHandler>* handlers)
{
    handlers_ = handlers;
}

Uint32 DUBU::SessionManager::GenerateId()
{
    Uint32 id = distribution_(rng_);
    while (sessionMap_.count(id) > 0)
    {
        id = distribution_(rng_);
    }
    return id;
}
