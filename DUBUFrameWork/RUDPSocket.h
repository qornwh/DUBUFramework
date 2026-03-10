#pragma once
#include "pch.h"
#include "RWLock.h"

namespace DUBU
{
	struct OverlappedPacketBuffer;

	// OnRecvFrom OnSendTo함수를 분리해 결합도를 낮추어본다.
	class ISocketHandler 
	{
	public:
		virtual ~ISocketHandler() {}

		virtual void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size) = 0;
		virtual void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size) = 0;
	};

	class RUDPSocket : public std::enable_shared_from_this<RUDPSocket>
	{
	public:
		RUDPSocket();
		RUDPSocket(const RUDPSocket& other) = delete;
		RUDPSocket(RUDPSocket&& other) = delete;
		~RUDPSocket();

		RUDPSocket& operator=(const RUDPSocket& other) = delete;
		RUDPSocket& operator=(RUDPSocket&& other) = delete;

		// 서버용
		void StartServer();
		void EndServer();

		// 클라용
		void StartClient(const String& serverIP, Uint16 serverPort);
		void EndClient();
		
		void SetSocketAddr(SOCKADDR_IN& serverAddr);
		SOCKADDR_IN& GetSockAddr();

		void Closesocket();

		Int32 Dispatch(LPOVERLAPPED* ptr, DWORD timeout = 10);
		void SetHandler(ISocketHandler* handler) { handler_ = handler; }
		void RecvFrom();
		void SendTo(const SOCKADDR_IN& targetAddr, Uint8* buffer, Uint16 size);
		void SendToReliable(const SOCKADDR_IN& targetAddr, Uint8* buffer, Uint16 size);
		void SendToRepeat(const SOCKADDR_IN& targetAddr, Uint8* buffer, Uint16 size);
		void RecvFromComplete(OVERLAPPED* ptr, Uint16 size);
		void SendToComplete(OVERLAPPED* ptr, Uint16 size);

	private:
		HANDLE iocpHd_;
		SOCKET socket_ = INVALID_SOCKET;
		Int32 port_ = SERVICE_PORT;
		Int32 firstClientCount_ = FIRST_CLIENT_COUNT;
		Lock lk_;
		bool isServer_ = false;
		ISocketHandler* handler_ = nullptr;

		SOCKADDR_IN serverAddr_;
		String serverIP_;
		Uint16 serverPort_;
	};
}

