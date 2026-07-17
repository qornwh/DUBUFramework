#pragma once
#include "pch.h"
#include "Config.h"
#include "Packet.h"
#include "RUDPSocket.h"
#include "PendingPacket.h"
#include "ReliablePacketState.h"

#include "ChunkPacket.h"

// 일단 최대 10번만 연결 시도
#define DEFAULT_REQUEST_CONNECT_NO 10

namespace DUBU
{
	class RUDPSocket;

    /*
    * Client : 베이스 클래스
    *  - 초기 커넥션
    *  - ping 수신 pong 전송
    *  - echo 송신
    */
	class Client : public ISocketHandler
	{
	public:
		Client(const String& serverIP, Uint16 serverPort, const Map<Uint8, Packet::PacketHandler>* handlers = nullptr);
		virtual ~Client();
        virtual void Reset();

		void Connect();
        void ConnectTimes(Uint32 count = 5);
		void Disconnect();
		bool Dispatch();
		void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size);
		void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size);
        const Bool IsConnect() const { return isConnect_; }
        Lock& GetLock() { return lock_; }

        void SendAck(Uint32 seqNo, const Packet::PacketOpctions& opt);

		void ConnectMessage();
        void DisconnectMessage();
		Uint32 GetClientId() const;

        bool RecvDispatch(Uint8* buffer, Uint16 size);
        bool RecvDispatchACK(Uint8* buffer, Uint16 size);
        // ping받고 pong으로 전달
        void RepeatPongMessage(Uint8* ptr, Uint16 size);
        void RepeatMessageAll(Uint32 resendDelay);
        void PacketParse(Uint8* buffer, Uint16 size);

        // 주기적 pendingMessage 전송 체크
        void CheckPending();

        // Echo 메시지 
        void SendEchoMessage();

        // SendPacket
        void SendPacket(Uint8* buffer, Uint8 code, Uint16 size, const Packet::PacketOpctions& opt, const Uint8* subHeader = nullptr, Uint16 subHeaderSize = 0);

    private:
        // SendPacketChunck
        void SendPacketChunk(Uint8* buffer, Uint8 code, Uint16 size, const Uint8* subHeader = nullptr, Uint16 subHeaderSize = 0);
        void RepeatMessage(Uint32 resendDelay, ReliablePacketState& rps, Uint32 now);

    protected:
        // 소켓만 protected로 수정
        std::shared_ptr<RUDPSocket> rudpSocket_;

        // 일단 멤버변수로 청크 패킷관리를 한다.
        ChunkPacketInj chunckPakcetInj_;

    private:
        // 순서보장 x 재전송 x 패킷 정보
        PacketStateBase rNopsNo_;
        // 순서보장 x 재전송 o 패킷 정보
        ReliablePacketState rpsNo_;
        // 채널 생성
        Vector<CacheAlreadyPacket> cacheAlreadyPackets_;

	private:
        // 클라이언트 id
		Uint32 clientId_ = 0;
        // 재전송 시도 횟수
        Uint32 retryCount_ = 0;

        // 초기값 0.5초
        Uint32 rttMillisec_ = g_defaultRttMs;
        // 가장 마지막 시간
        Uint32 timestamp_ = 0;

        // 패킷별 함수 분기대신 Map사용
        const Map<Uint8, Packet::PacketHandler>* handlers_;

        // 연결 관리
        Bool isConnect_ = false;

        // 핑퐁 시퀀스넘버 체크 변수
        Uint32 lastPongSeq_ = 0;

        // 커넥션 카운트 체크 변수
        Uint32 connNo_ = 0;
        
        // 클라이언트 타임아웃 설정
        const Uint32 ClientTimeout = g_defaultDisconnectTimeoutMs;

        // 클라이언트 lock
        Lock lock_;
	};
}

