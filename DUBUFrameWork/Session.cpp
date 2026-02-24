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

void DUBU::Session::SetSessionId(Int32 sessionId)
{
	sessionId_ = sessionId;
}

void DUBU::Session::Reset()
{
	sessionId_ = 0;
	recvSequenceNo_ = 0;
	sendSequenceNo_ = 0;
	timestamp_ = 0;

	rttMillisec_ = DEFAULT_RTT_MS;
}

bool DUBU::Session::RecvDispatch(Uint8* buffer, Int32 size)
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

bool DUBU::Session::RecvDispatchACK(Uint8* buffer, Int32 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	// 이전 패킷 중복 넘김
	if (header->sequenceNo_ <= recvSequenceNo_)
	{
		return false;
	}
	
	// 받아온 패킷으로 늘려준다.
	if (header->sequenceNo_ == recvSequenceNo_ + 1)
	{
		recvSequenceNo_ = header->sequenceNo_;
	}

	/*
	* 여기서 추가적으로 서버 -> 클라로 데이터 줄 때 (예를들면 아이템 구매)
	* 확인패킷이 어떤 패킷인지 분석후 처리결과를 클라로 던져야 된다.
	*/

	return true;
}

void DUBU::Session::Send(Uint32 sequenceNo)
{
}

void DUBU::Session::SendAck(Uint32 sequenceNo)
{
}
