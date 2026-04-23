#pragma once
#include "pch.h"
#include "RWLock.h"
#include "RUDPSocket.h"
#include "SessionManager.h"

namespace DUBU 
{
    /*
    * Server 객체에서만 SessionManager관리한다.
    * 그러므로 SessionManager에서 Lock을 제거하고 Server에서 Lock처리한다.
    * CheckSession <= 일정 주기마다 모든 세션의 재전송 패킷 전송 및 타임아웃 체크
    */
	class RUDPSocket;

	class Server : public ISocketHandler
	{
	public:
		Server();
		virtual ~Server();

		void Initialize();
        // 계속실행
		void Run();
        // 1회 실행
        void Dispatch();
		void Stop();

		void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size) override;
		void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size) override;

		virtual Session* CreateSession(const SOCKADDR_IN& addr);
		void ConnectMessage(Session* session);
		void DisconnectMessage(Session* session);
		void SendPing(Session* session);
		void CheckSession();

        void SendAck(Uint32 seqNo, Session* session);

	private:
		Uint64 PeerKey(const SOCKADDR_IN& addr);

	private:
		SessionManager sessionManager_;
		std::shared_ptr<RUDPSocket> rudpSocket_;
		Lock sessionLock_;
		Bool isRunning_ = false;

        // Peer 리스트 관리
		Map<Uint64, Peer> peerMap_;

        // CheckSession 1스레드만 작동되도록 CAS연산
		Atomic<Bool> sessionCAS_;

		// Ping전달 시간 체크
		const Int32 PingTimeout = DEFAULT_PING_TIMEOUT_MS;

		// 세션 타임아웃 설정
		const Int32 SessionTimeout = DEFAULT_DISCONNECT_TIMEOUT_MS;

		// 끊을 세션 캐시로 저장
		Uint32 removeListCache_[MAX_CLIENT_COUNT];
	};
}

