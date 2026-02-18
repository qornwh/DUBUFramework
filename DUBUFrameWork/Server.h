#pragma once
#include "pch.h"
#include "RWLock.h"
#include "RUDPSocket.h"
#include "SessionManager.h"

namespace DUBU 
{
	class Server : public ISocketHandler
	{
	public:
		Server();
		virtual ~Server();

		void Initialize();
		void Run();

		void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size);
		void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size);

		// 技记 包府(技记 概聪历 积己)
		//  - 犁傈价 菩哦甸 包府
		

	private:
		std::shared_ptr<RUDPSocket> rudpServer_;
		Lock lk_;
		bool isRunning_ = false;

		SessionManager sessionManager_;
	};
}

