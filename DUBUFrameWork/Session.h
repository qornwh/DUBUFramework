#pragma once
#include "pch.h"
#include "Packet.h"

namespace DUBU
{
	class Session
	{
	public:
		Session(const Map<Uint8, Packet::PacketHandler>* handlers);
		virtual ~Session();

		void SetSockAddr(const SOCKADDR_IN& addr);
		void SetSessionId(Int32 sessionId);
		virtual void Reset();

		// 실제 recv되면 패킷 파싱 후 Recv 호출
		bool RecvDispatch(Uint8* buffer, Int32 size);
		bool RecvDispatchACK(Uint8* buffer, Int32 size);

		// send는 재전송 패킷용, 보내는용 나눈다.
		void Send(Uint32 sequenceNo);
		void SendAck(Uint32 sequenceNo);

	private:
		Uint32 sessionId_ = 0;
		Uint32 recvSequenceNo_ = 0;
		Uint32 sendSequenceNo_ = 0;

		// 초기값 0.5초
		Uint32 rttMillisec_ = DEFAULT_RTT_MS;

		// 가장 마지막 시간
		Uint32 timestamp_ = 0;

		// ip port바인딩 => ip바껴도 sessionID로 판별하기 때문에 유지가능
		SOCKADDR_IN addr_;

		// 패킷별 함수 분기대신 Map사용
		const Map<Uint8, Packet::PacketHandler>* handlers_;
	};
}
