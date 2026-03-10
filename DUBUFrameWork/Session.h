#pragma once
#include "pch.h"
#include "Packet.h"
#include "RingQueue.h"

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
		// 외부에서 수정 불가능 하게 만든다.
		const SOCKADDR_IN& GetSockAddr() const;
		void SetSessionId(Int32 sessionId);
		Int32 GetSessionId() const;

		// 외부 참조용
		Uint32 GetRecvSequenceNo() const;
		Uint32 GetSendSequenceNo() const;
		Uint32 GetRetryCount() const;
		Uint32 GetRttMillisec() const;
		Int64 GetTimestamp() const;
		DS::RingQueue<std::tuple<Int64, Uint32, Uint8*>>& GetPendingQueue();

		bool RecvDispatch(Uint8* buffer, Uint16 size);
		bool RecvDispatchACK(Uint8* buffer, Uint16 size);

	private:
		Uint32 sessionId_ = 0;
		Uint32 recvSequenceNo_ = 0;
		Uint32 sendSequenceNo_ = 0;
		Uint32 retryCount_ = 0;

		// 초기값 0.5초
		Uint32 rttMillisec_ = DEFAULT_RTT_MS;

		// 가장 마지막 시간
		Int64 timestamp_ = 0;

		// ip port바인딩 => ip바껴도 sessionID로 판별하기 때문에 유지가능
		SOCKADDR_IN addr_;

		// 재전송 패킷 리스트
		// 시간, 넘버, 패킷 포인터 
		DS::RingQueue<std::tuple<Int64, Uint32, Uint8*>> pendingQueue_;

		// 패킷별 함수 분기대신 Map사용
		const Map<Uint8, Packet::PacketHandler>* handlers_;
	};
}
