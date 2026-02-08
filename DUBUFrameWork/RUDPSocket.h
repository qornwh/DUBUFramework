#pragma once
#include "pch.h"
#include "RWLock.h"

namespace DUBU
{
	class RUDPSocket : public std::enable_shared_from_this<RUDPSocket>
	{
	public:
		RUDPSocket();
		RUDPSocket(const RUDPSocket& other) = delete;
		RUDPSocket(RUDPSocket&& other) = delete;
		~RUDPSocket();

		RUDPSocket& operator=(const RUDPSocket& other) = delete;
		RUDPSocket& operator=(RUDPSocket&& other) = delete;

		void StartServer();
		void EndServer();

		Int32 Dispatch(LPOVERLAPPED* ptr, DWORD timeout = 10);
		void RecvFrom();
		void SendTo(const SOCKADDR_IN& targetAddr, Byte* buffer, Int32 size);
		void RecvFromComplete(OVERLAPPED* ptr, Int32 size);
		void SendToComplete(OVERLAPPED* ptr, Int32 size);

	private:
		HANDLE iocpHd_;
		SOCKET serverSocket_ = INVALID_SOCKET;
		Int32 port_ = SERVICE_PORT;
		Int32 firstClientCount_ = FIRST_CLIENT_COUNT;
		Lock lk_;
		bool isServer_ = false;
	};
}

