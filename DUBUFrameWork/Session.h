#pragma once
#include "pch.h"
#include "Packet.h"
#include "Peer.h"

namespace DUBU
{
	/*
	* Session 클래스에서 Recv, Send를 직접 담당하지는 않는다.
	*/
	class Session
	{
		/*
		* 재전송 패킷 정보
		*/
		struct PendingPacket {
			struct OverlappedPacketBuffer* buffer = nullptr;
			Int64   timeStamp = 0;
			Uint32  sequenceNo = 0;
			bool    isSent = false;
		};

	public:
		Session(const Map<Uint8, Packet::PacketHandler>* handlers);
		virtual ~Session();
		virtual void Reset();

		void SetSockAddr(const SOCKADDR_IN& addr);
		void SetSessionId(Int32 sessionId);
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
		void RepeatACK(class RUDPSocket* socket, Int64 resendDelay);
		void SetPeer(Peer& peer);
		void AddPendingPacket(Uint8* buffer, Uint16 size);

		void Disconnect();

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

		// 연결 해제됨
		Bool isConnect_;
	};
}
