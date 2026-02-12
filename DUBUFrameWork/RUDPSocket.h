#pragma once
#include "pch.h"
#include "RWLock.h"
#include "ConCurrentQueue.h"

namespace DUBU
{
	struct OverlappedPacketBuffer;

	// OnRecvFrom OnSendTo함수를 분리해 결합도를 낮추어본다.
	class ISocketHandler 
	{
	public:
		virtual void OnRecvFrom(const SOCKADDR_IN& addr, Byte* buffer, Int32 size) = 0;
		virtual void OnSendTo(const SOCKADDR_IN& addr, Int32 size) = 0;
	};

	class RUDPSocket : public std::enable_shared_from_this<RUDPSocket>
	{
	public:
		RUDPSocket();
		RUDPSocket(const RUDPSocket& other) = delete;
		RUDPSocket(RUDPSocket&& other) = delete;
		~RUDPSocket();

		void SetHandler(std::shared_ptr<ISocketHandler> handler) { handler_ = handler; }

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

		std::shared_ptr<ISocketHandler> handler_;

		// 재전송 메시지 큐
		DS::ConcurrentQueue<OverlappedPacketBuffer*> q_;
		Uint32 sequnceNumber_ = 0;
	};
}

