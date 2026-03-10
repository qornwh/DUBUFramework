#include "Session.h"
#include "Packet.h"
#include "../extra/base_flatbuffer_generated.h"

DUBU::Session::Session(const Map<Uint8, Packet::PacketHandler>* handlers) : 
	handlers_(handlers), sessionId_(0), recvSequenceNo_(0), sendSequenceNo_(0), timestamp_(0), addr_(), rttMillisec_(DEFAULT_RTT_MS)
{
}

DUBU::Session::~Session()
{
}

void DUBU::Session::SetSockAddr(const SOCKADDR_IN& addr)
{
	addr_ = addr;
}

const SOCKADDR_IN& DUBU::Session::GetSockAddr() const
{
	return addr_;
}

void DUBU::Session::SetSessionId(Int32 sessionId)
{
	sessionId_ = sessionId;
}

Int32 DUBU::Session::GetSessionId() const
{
	return sessionId_;
}

Uint32 DUBU::Session::GetRecvSequenceNo() const
{
	return recvSequenceNo_;
}

Uint32 DUBU::Session::GetSendSequenceNo() const
{
	return sendSequenceNo_;
}

Uint32 DUBU::Session::GetRetryCount() const
{
	return retryCount_;
}

Uint32 DUBU::Session::GetRttMillisec() const
{
	return rttMillisec_;
}

Int64 DUBU::Session::GetTimestamp() const
{
	return timestamp_;
}

DUBU::DS::RingQueue<std::tuple<Int64, Uint32, Uint8*>>& DUBU::Session::GetPendingQueue()
{
	return pendingQueue_;
}

void DUBU::Session::Reset()
{
	sessionId_ = 0;
	recvSequenceNo_ = 0;
	sendSequenceNo_ = 0;
	timestamp_ = 0;

	rttMillisec_ = DEFAULT_RTT_MS;
}

bool DUBU::Session::RecvDispatch(Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	// 이전 패킷 중복 넘김
	if (header->sequenceNo_ <= recvSequenceNo_)
	{
		return false;
	}

	// 패킷  체크
	flatbuffers::Verifier verifier(buffer + sizeof(Packet::PacketHeader), size);
	Uint8 packetCode = header->packetCode_;
	
	auto it = handlers_->find(packetCode);
	if (it == handlers_->end())
	{
		// 패킷코드에 대한 함수가 등록되지 않음
		spdlog::error("Not Found PacketCode : {} !!!", packetCode);
		return false;
	}

	if (!it->second.verifier_(verifier))
	{
		// 패킷이 정확하지 않음
		spdlog::warn("Verfiy Failed !!!");
		return false;
	}

	// 패킷별 함수 실행
	it->second.handler_(buffer, size);
	return true;
}

bool DUBU::Session::RecvDispatchACK(Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	// 이전 패킷 중복 넘김
	if (header->sequenceNo_ <= sendSequenceNo_)
	{
		return false;
	}
	
	// 받아온 패킷으로 늘려준다.
	if (header->sequenceNo_ == sendSequenceNo_ + 1)
	{
		recvSequenceNo_ = header->sequenceNo_;
	}

	return true;
}
