#pragma once
#include "pch.h"
#include <random>
#include "Session.h"
#include "RWLock.h"

namespace DUBU
{
	class SessionManager
	{
		// 여기서만 세션 생성, 등록, 삭제를 관리 / 다른곳에서 금지
	public:
		SessionManager();
		SessionManager(const SessionManager& other) = delete;
		SessionManager(SessionManager&& other) = delete;
		~SessionManager();

		SessionManager& operator=(const SessionManager& other) = delete;
		SessionManager& operator=(SessionManager&& other) = delete;

        template<std::derived_from<Session> T>
		T* AddSession();
        template<std::derived_from<Session> T>
        T* GetSession(Uint32 sessionId);

        Session* AddSession();
        Session* GetSession(Uint32 sessionId);
		Map<Uint32, Session*>& GetSessions();
        void RemoveSession(Uint32 sessionId);

        void SetHandlers(const Map<Uint8, Packet::PacketHandler>* handlers);

	private:
		Uint32 GenerateId();

		std::mt19937 rng_ { std::random_device{}() };
		std::uniform_int_distribution<Uint32> distribution_{ (Uint32)1, ((Uint32)0) - 1 };
		Map<Uint32, Session*> sessionMap_;
		Atomic<Uint32> sessionCount_{ 0 };

        // 세션핸들러
        const Map<Uint8, Packet::PacketHandler>* handlers_;
	};
    template<std::derived_from<Session> T>
    inline T* SessionManager::AddSession()
    {
        Uint32 sessionId = GenerateId();
        sessionCount_.fetch_add(1);
        T* session = Pop<T>(handlers_);
        session->Reset();
        session->SetSessionId(sessionId);
        sessionMap_.insert({ sessionId, static_cast<Session>(session) });

        return session;
    }
    template<std::derived_from<Session> T>
    inline T* SessionManager::GetSession(Uint32 sessionId)
    {
        auto it = sessionMap_.find(sessionId);
        if (it != sessionMap_.end())
        {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }
}

