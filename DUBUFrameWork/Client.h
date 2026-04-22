#pragma once
#include "pch.h"
#include "Packet.h"
#include "RUDPSocket.h"
#include "PendingPacket.h"

// 일단 최대 10번만 연결 시도
#define DEFAULT_REQUEST_CONNECT_NO 10

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

		void Connect();
		void Disconnect();
		bool Dispatch();
		void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size);
		void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size);
        Uint32 UpdateSendSequenceNo();

		void ConnectMessage();
        void DisconnectMessage();
		void SendEchoMessage();
		Uint32 GetClientId() const;

        bool RecvDispatch(Uint8* buffer, Uint16 size);
        bool RecvDispatchACK(Uint8* buffer, Uint16 size);
        void RepeatMessage(Uint64 resendDelay);
        void AddPendingPacket(Uint8* buffer, Uint16 size);

        void SendAck(Uint32 seqNo);

        // ping받고 pong으로 전달
        void RepeatPongMessage(Uint8* ptr, Uint16 size);

        // 주기적 pendingMessage 전송 체크
        void CheckPending();

	private:
        // 클라이언트 id
		Uint32 clientId_ = 0;
        // 수신 시퀀스 넘버
        Uint32 recvSequenceNo_ = 0;
        // 송신 시퀀스 넘버
        Uint32 sendSequenceNo_ = 0;

		std::shared_ptr<RUDPSocket> rudpSocket_;

        // 재전송 패킷 관리
        PendingPacket pendingPackets_[DEFAULT_WINDOW_COUNT];
        Uint32 localWindowStart_ = 0;
        Uint32 localSeqence_ = 0;
        // 초기값 0.5초
        Uint32 rttMillisec_ = DEFAULT_RTT_MS;
        // 가장 마지막 시간
        Uint64 timestamp_ = 0;

        // 패킷별 함수 분기대신 Map사용
        const Map<Uint8, Packet::PacketHandler>* handlers_;

        // 연결 관리
        Bool isConnect_ = false;

        // 핑퐁 시퀀스넘버 체크 변수
        Uint32 lastPongSeq_ = 0;

        // 커넥션 카운트 체크 변수
        Uint32 connNo_ = 0;
        
        // 클라이언트 타임아웃 설정
        const Int32 ClientTimeout = DEFAULT_DISCONNECT_TIMEOUT_MS;
	};
}

