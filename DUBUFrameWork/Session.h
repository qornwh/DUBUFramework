#pragma once
#include "pch.h"
#include "Config.h"
#include "Packet.h"
#include "Peer.h"
#include "RWLock.h"
#include "ReliablePacketState.h"

#include "ChunkPacket.h"

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

		const SOCKADDR_IN& GetSockAddr() const;
        Uint32 GetSessionId() const;
		Uint32 GetRetryCount() const;
		Uint32 GetRttMillisec() const;
		Uint32 GetTimestamp() const;
		Bool IsConnection() const;

		bool RecvDispatch(Uint8* buffer, Uint16 size);
		bool RecvDispatchACK(Uint8* buffer, Uint16 size);
		bool RecvDispatchPong(Uint8* buffer, Uint16 size);
		void RepeatMessageAll(class RUDPSocket* socket, Uint32 resendDelay);
        void PacketParse(Uint8* buffer, Uint16 size);

        // ACK 리턴
        void SendACK(Uint32 seqNo, const Packet::PacketOpctions& opt);

		Uint32 GetLastPingSentTime() const;
		void SetLastPingSentTime(Uint32 time);
		void SetPeer(Peer& peer);

		virtual void Disconnect();

        // 핑 카운트 기록
        void AddPingCount() { ++pingCount_; };
        Uint32 GetPingCount() const { return pingCount_; };
        void AddPongCount() { ++pongCount_; };
        Uint32 GetPongCount() const { return pongCount_; };
        Uint32 AccSequnceNo() { return ++lastPongSeq_; }

        // Echo 메시지 
        void SendEchoMessage(Uint8* buffer, Uint16 size);

        // SendPacket
        void SendPacket(Uint8* buffer, Uint8 code, Uint16 size, const Packet::PacketOpctions& opt, const Uint8* subHeader = nullptr, Uint16 subHeaderSize = 0);
        
        void SetAwaysConnect(Bool awaysConnect);
        Bool GetAwaysConnect() const { return awaysConnect_; };
        
        void SetReadyDisconnect(Bool readyDisconnect);
        Bool GetReadyDisconnect() const { return readyDisconnect_; };

    protected:
        // 일단 멤버변수로 청크 패킷관리를 한다.
        ChunkPacketInj chunckPakcetInj_;

    private:
        // 청크 패킷용 처리
        void RecvChunckPacket(Uint8* buffer, Uint16 size);
        // SendPacketChunck
        void SendPacketChunk(Uint8* buffer, Uint8 code, Uint16 size, const Uint8* subHeader = nullptr, Uint16 subHeaderSize = 0);
        void RepeatMessage(RUDPSocket* socket, Uint32 resendDelay, ReliablePacketState& rps, Uint32 now);

    private:
        // 순서보장 x 재전송 x 패킷 정보 (lastRepeatSeq_는 사용하지 않는다)
        PacketStateBase rNopsNo_;
        // 순서보장 x 재전송 o 패킷 정보
        ReliablePacketState rpsNo_;
        // 채널 생성
        Vector<CacheAlreadyPacket> cacheAlreadyPackets_;

	private:
		// 세션 id
		Uint32 sessionId_ = 0;
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

		// 패킷별 함수 분기대신 Map사용
		const Map<Uint8, Packet::PacketHandler>* handlers_;

		// 연결 관리
		Bool isConnect_;

        // 핑 카운트 기록
        Uint32 pingCount_ = 0;
        Uint32 pongCount_ = 0;
        Uint32 lastPongSeq_ = 0;

#ifdef TEST_MODE
        // 재전송 패킷 기록
        Atomic<Uint64> resendCount_;
#endif

        // RUDP소켓은 raw pointer로 관리
        RUDPSocket* rudpSocket_;

        // SendPacket 전용 lock
        Lock sendLock_;

        // 상시 세션
        Bool awaysConnect_ = false;

        // 재전송 오버플로우 끊기용.
        Bool readyDisconnect_ = false;
	};
}
