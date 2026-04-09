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

		Session* AddSession();
		void RemoveSession(Uint32 sessionId);
		Session* GetSession(Uint32 sessionId);
		Map<Uint32, Session*>& GetSessions();

	private:
		Uint32 GenerateId();

		std::mt19937 rng_ { std::random_device{}() };
		std::uniform_int_distribution<Uint32> distribution_{ (Uint32)1, ((Uint32)0) - 1 };
		Map<Uint32, Session*> sessionMap_;
		Atomic<Uint32> sessionCount_{ 0 };
	};
}

