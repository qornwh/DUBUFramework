#pragma once
#include "pch.h"
#include "RWLock.h"
#include "SessionManager.h"

namespace DUBU 
{
	class RUDPSocket;

	class Server : public ISocketHandler
	{
	public:
		Server();
		virtual ~Server();

		void Initialize();
		void Run();

		void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size) override;
		void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size) override;

		// 技记 包府(技记 概聪历 积己)
		//  - 犁傈价 菩哦甸 包府
		virtual void CreateSession() {};


	private:
		std::shared_ptr<RUDPSocket> rudpSocket_;
		Lock lk_;
		bool isRunning_ = false;

		SessionManager sessionManager_;
	};
}

