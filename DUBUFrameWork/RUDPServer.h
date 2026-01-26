#pragma once
#include "pch.h"
#include "RWLock.h"

namespace DUBU
{
	class RUDPServer : public std::enable_shared_from_this<RUDPServer>
	{
	public:
		RUDPServer();
		RUDPServer(const RUDPServer& other) = delete;
		RUDPServer(RUDPServer&& other) = delete;
		~RUDPServer();

		RUDPServer& operator=(const RUDPServer& other) = delete;
		RUDPServer& operator=(RUDPServer&& other) = delete;

		void Start();
		void Update();
		void End();
		Bool RecvFrom();
		Bool SendTo();

		Uint32 CreateNumber();

	private:
		HANDLE iocpHd_;
		SOCKET serverSocket_ = INVALID_SOCKET;
		Int32 port_ = SERVICE_PORT;
		Int32 firstClientCount_ = FIRST_CLIENT_COUNT;
		Lock lk_;
	};
}

