#pragma once
#include "pch.h"
#include "Config.h"
#include "Packet.h"
#include "Peer.h"
#include "PendingPacket.h"

namespace DUBU
{
    struct OverlappedPacketBuffer;

	/*
	* Session : 베이스 클래스
    *  - 핑 카운트 기록
    *  - reable패킷 전송률 기록
    *  - echo 송신
	*/
	class Session
	{
	public:
		Session(const Map<Uint8, Packet::PacketHandler>* handlers);
		virtual ~Session();
		virtual void Reset();

		void SetSockAddr(const SOCKADDR_IN& addr);
		void SetSessionId(Int32 sessionId);
        void SetSocket(class RUDPSocket* socket);
		void SetTimestamp(const Uint32 time);
		Uint32 UpdateSendSequenceNo();

		const SOCKADDR_IN& GetSockAddr() const;
        Uint32 GetSessionId() const;
		Uint32 GetRecvSequenceNo() const;
		Uint32 GetSendSequenceNo() const;
		Uint32 GetRetryCount() const;
		Uint32 GetRttMillisec() const;
		Uint32 GetTimestamp() const;
		Bool IsConnection() const;

		bool RecvDispatch(Uint8* buffer, Uint16 size);
		bool RecvDispatchACK(Uint8* buffer, Uint16 size);
		bool RecvDispatchPong(Uint8* buffer, Uint16 size);
		void RepeatMessage(class RUDPSocket* socket, Uint32 resendDelay);

        // ACK 리턴
        void SendACK(Uint32 seqNo);

		Uint32 GetLastPingSentTime() const;
		void SetLastPingSentTime(Uint32 time);
		void SetPeer(Peer& peer);
		void AddPendingPacket(OverlappedPacketBuffer* opb, Uint16 size);

		void Disconnect();

        // 핑 카운트 기록
        void AddPingCount() { ++pingCount_; };
        Uint32 GetPingCount() const { return pingCount_; };
        void AddPongCount() { ++pongCount_; };
        Uint32 GetPongCount() const { return pongCount_; };
        Uint32 AccSequnceNo() { return ++lastPongSeq_; }

        // Echo 메시지
        void SendEchoMessage(Uint8* buffer, Uint16 size);

        void SendPacket(Uint8* buffer, Uint8 code, Uint16 size);
        void SendPacketNoReliable(Uint8* buffer, Uint8 code, Uint16 size, const Uint8* subHeader = nullptr, Uint16 subHeaderSize = 0);
        void SendPacketReliable(Uint8* buffer, Uint8 code, Uint16 size, const Uint8* subHeader = nullptr, Uint16 subHeaderSize = 0);

        void SetAwaysConnect(Bool awaysConnect);
        Bool GetAwaysConnect() const { return awaysConnect_; };

    private:
        // 미리온 repeat 패킷은 캐싱해둠. 그후 이전 번호까지 되면 그거 가져와서 실행함.
        Uint64 currentRepeatNoOrder_ = 0;
        Uint64 lastRepeatNoOrder_ = 0;
        Uint64 cacheRepeatNoOrder_ = 0;

        // 0번은 repeat 순서 보장 x이다, 이외에는 모두 채널번호다.
        Vector<OverlappedPacketBuffer*> CacheAlreadyPackets_[1 << 6];

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
		Uint32 rttMillisec_ = g_defaultRttMs;
		// 가장 마지막 시간
		Uint32 timestamp_ = 0;

		// 마지막으로 Ping을 보낸 시간 (throttle용)
		Uint32 lastPingSentTime_ = 0;

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

#ifdef _DEBUG
        // 재전송 패킷 기록
        Atomic<Uint64> resendCount_;
#endif

        // RUDP소켓은 raw pointer로 관리
        RUDPSocket* rudpSocket_;

        // 상시 세션
        Bool awaysConnect_ = false;
	};
}
