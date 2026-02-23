#include "Session.h"
#include "Packet.h"
#include "../extra/base_flatbuffer_generated.h"

DUBU::Session::Session() : sessionId_(0), recvSequenceNo_(0), sendSequenceNo_(0), timestamp_(0), addr_(), rttMillisec_(DEFAULT_RTT_MS)
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
	switch (header->packetCode_)
	{
	case 1:
		if (!Base::Sample::VerifyPacketBuffer(verifier)) return false;
		Recv(buffer, size);
		break;
	default:
		return false;
	}

	return true;
}

void DUBU::Session::Recv(Uint8* buffer, Int32 size)
{
}

void DUBU::Session::Send(Uint32 sequenceNo)
{
}

void DUBU::Session::SendAck(Uint32 sequenceNo)
{
}
