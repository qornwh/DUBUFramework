#pragma once
#include "pch.h"
#include "Config.h"
#include "Packet.h"
#include "RUDPSocket.h"
#include "PendingPacket.h"

// 일단 최대 10번만 연결 시도
#define DEFAULT_REQUEST_CONNECT_NO 10

namespace DUBU
{
    class RUDPSocket;

    /*
    * InternalClient : 서버간 연결할 베이스 클래스
    *  - 서버 - 서버 연결에 사용할 클라이언트 / 기존 베이스 클라이언트와 다름
    */

    class InternalClient : public ISocketHandler
    {
    public:
        InternalClient(const String& serverIP, Uint16 serverPort, const Map<Uint8, Packet::PacketHandler>* handlers = nullptr);
        virtual ~InternalClient();

        void Connect();
        void ConnectTimes(Uint32 count = 5);
        void Disconnect();
        bool Dispatch();
        void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size);
        void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size);
        const Bool IsConnect() const { return isConnect_; }
        Uint32 UpdateSendSequenceNo();

        void ConnectMessage();
        void DisconnectMessage();
        Uint32 GetClientId() const;

        bool RecvDispatch(Uint8* buffer, Uint16 size);
        bool RecvDispatchACK(Uint8* buffer, Uint16 size);
        void RepeatMessage(Uint32 resendDelay);
        void AddPendingPacket(OverlappedPacketBuffer* opb, Uint16 size);

        void SendPacket(Uint8* buffer, Uint8 code, Uint16 size);
        void SendAck(Uint32 seqNo);

        // ping받고 pong으로 전달
        void RepeatPongMessage(Uint8* ptr, Uint16 size);

        // 주기적 pendingMessage 전송 체크
        void CheckPending();

        Uint32 GetRecvSequenceNo() const;

        Lock& GetLock() { return lock_; }

    protected:
        // 소켓만 protected로 수정
        std::shared_ptr<RUDPSocket> rudpSocket_;

    private:
        // 클라이언트 id
        Uint32 clientId_ = 0;
        // 수신 시퀀스 넘버
        Uint32 recvSequenceNo_ = 0;
        // 송신 시퀀스 넘버
        Uint32 sendSequenceNo_ = 0;

        // 재전송 패킷 관리
        PendingPacket pendingPackets_[DEFAULT_WINDOW_COUNT];
        Uint32 localWindowStart_ = 0;
        Uint32 localSeqence_ = 0;
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

        // 타임아웃 설정
        const Uint32 ClientTimeout = g_defaultDisconnectTimeoutMs;

        Lock lock_;
    };
}

