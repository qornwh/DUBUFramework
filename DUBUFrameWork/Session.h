#pragma once
#include "pch.h"
#include "Packet.h"
#include "Peer.h"
#include "PendingPacket.h"

namespace DUBU
{
	/*
	* Session 클래스에서 Recv, Send를 직접 담당하지는 않는다.
	*/
	class Session
	{
	public:
		Session(const Map<Uint8, Packet::PacketHandler>* handlers);
		virtual ~Session();
		virtual void Reset();

		void SetSockAddr(const SOCKADDR_IN& addr);
		void SetSessionId(Int32 sessionId);
		void SetTimestamp(const Uint64 time);
		Int32 UpdateSendSequenceNo();

		const SOCKADDR_IN& GetSockAddr() const;
		Int32 GetSessionId() const;
		Uint32 GetRecvSequenceNo() const;
		Uint32 GetSendSequenceNo() const;
		Uint32 GetRetryCount() const;
		Uint32 GetRttMillisec() const;
		Uint64 GetTimestamp() const;
		Bool IsConnection() const;

		bool RecvDispatch(Uint8* buffer, Uint16 size);
		bool RecvDispatchACK(Uint8* buffer, Uint16 size);
		bool RecvDispatchPong(Uint8* buffer, Uint16 size);
		void RepeatMessage(class RUDPSocket* socket, Uint64 resendDelay);

        // ACK 리턴
        void SendACK(Uint32 seqNo);

		Uint64 GetLastPingSentTime() const;
		void SetLastPingSentTime(Uint64 time);
		void SetPeer(Peer& peer);
		void AddPendingPacket(Uint8* buffer, Uint16 size);

		void Disconnect();

        // 핑 카운트 기록
        void AddPingCount() { ++pingCount_; };
        Uint32 GetPingCount() const { return pingCount_; };
        void AddPongCount() { ++pongCount_; };
        Uint32 GetPongCount() const { return pongCount_; };
        Uint32 AccSequnceNo() { return lastPongSeq_++; }

	private:
		// 세션 id
		Uint32 sessionId_ = 0;
		// 수신 시퀀스 넘버
		Uint32 recvSequenceNo_ = 0;
		// 송신 시퀀스 넘버
		Uint32 sendSequenceNo_ = 0;
		// 재전송 시도 횟수
		Uint32 retryCount_ = 0;

		// 초기값 0.5초
		Uint32 rttMillisec_ = DEFAULT_RTT_MS;
		// 가장 마지막 시간
		Uint64 timestamp_ = 0;

		// 마지막으로 Ping을 보낸 시간 (throttle용)
		Uint64 lastPingSentTime_ = 0;

		// ip port바인딩 => ip바껴도 sessionID로 판별하기 때문에 유지가능
		SOCKADDR_IN addr_;

		// Peer 관리
		Peer peer_;

		// 재전송 패킷 관리
		PendingPacket pendingPackets_[DEFAULT_WINDOW_COUNT];
		Uint32 localWindowStart_ = 0;
		Uint32 localSeqence_ = 0;

		// 패킷별 함수 분기대신 Map사용
		const Map<Uint8, Packet::PacketHandler>* handlers_;

		// 연결 관리
		Bool isConnect_;

        // 핑 카운트 기록
        Uint32 pingCount_ = 0;
        Uint32 pongCount_ = 0;
        Uint32 lastPongSeq_ = 0;
	};
}
