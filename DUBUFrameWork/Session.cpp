#include "Session.h"
#include "Packet.h"
#include "RUDPSocket.h"
#include "BufferManager.h"
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
	isConnect_ = true;
}

const SOCKADDR_IN& DUBU::Session::GetSockAddr() const
{
	return addr_;
}

void DUBU::Session::SetSessionId(Int32 sessionId)
{
	sessionId_ = sessionId;
}

Int32 DUBU::Session::UpdateSendSequenceNo()
{
	return ++sendSequenceNo_;
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

Uint64 DUBU::Session::GetTimestamp() const
{
	return timestamp_;
}

Bool DUBU::Session::IsConnection() const
{
	return isConnect_;
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
	Uint32 ackSeq = header->sequenceNo_;
	int idx = ackSeq % DEFAULT_WINDOW_COUNT;

	if (pendingPackets_[idx].sequenceNo == ackSeq && pendingPackets_[idx].buffer != nullptr)
	{
		// RTT 갱신 : 비율 4 : 1
		Int64 rtt = GetCurrentTimeMs() - pendingPackets_[idx].timeStamp;
		rttMillisec_ = (Uint32)(rttMillisec_ * 0.8f + rtt * 0.2f);

		// 수신 성공 버퍼 지운다.
		OverlappedPacketBuffer* pandingbuffer = pendingPackets_[idx].buffer;
		PacketManager::GetInstance().PushPacketBuffer(pandingbuffer);
		pendingPackets_[idx].buffer = nullptr;

		// pandding된 버퍼가 있는곳 까지 지운다, 단 localSeqence_까지만
		while (localWindowStart_ != localSeqence_ && pendingPackets_[localWindowStart_ % DEFAULT_WINDOW_COUNT].buffer == nullptr)
		{
			localWindowStart_++;
		}
	}
	return true;
}

void DUBU::Session::RepeatACK(RUDPSocket* socket, Int64 resendDelay)
{
	Uint64 now = GetCurrentTimeMs();

	for (Uint32 i = localWindowStart_; i < localSeqence_; ++i)
	{
		PendingPacket& p = pendingPackets_[i % DEFAULT_WINDOW_COUNT];
		if (p.buffer != nullptr && now - p.timeStamp >= resendDelay)
		{
			socket->SendToRepeat(addr_, p.buffer->buffer_, p.buffer->size_);
			p.timeStamp = now;
		}
	}
}

void DUBU::Session::SetPeer(Peer& peer)
{
	peer_ = peer;
}

void DUBU::Session::AddPendingPacket(Uint8* buffer, Uint16 size)
{
}

void DUBU::Session::Disconnect()
{
	isConnect_ = false;
}
