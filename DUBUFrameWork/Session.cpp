#include "Session.h"
#include "RUDPSocket.h"
#include "BufferManager.h"
#include "../extra/base_flatbuffer_generated.h"

DUBU::Session::Session(const Map<Uint8, Packet::PacketHandler>* handlers) :
	handlers_(handlers), sessionId_(0), recvSequenceNo_(0), sendSequenceNo_(0), timestamp_(0), lastPingSentTime_(0), addr_(), rttMillisec_(DEFAULT_RTT_MS), isConnect_(false)
{
}

DUBU::Session::~Session()
{
}

void DUBU::Session::SetSockAddr(const SOCKADDR_IN& addr)
{
	addr_ = addr;
	isConnect_ = true;
	timestamp_ = DUBU::GetCurrentTimeMs();
}

const SOCKADDR_IN& DUBU::Session::GetSockAddr() const
{
	return addr_;
}

void DUBU::Session::SetSessionId(Int32 sessionId)
{
	sessionId_ = sessionId;
}

void DUBU::Session::SetSocket(RUDPSocket* socket)
{
    rudpSocket_ = socket;
}

void DUBU::Session::SetTimestamp(const Uint64 time)
{
	timestamp_ = time;
}

Uint32 DUBU::Session::UpdateSendSequenceNo()
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
	// 세션 초기화
	sessionId_ = 0;
	recvSequenceNo_ = 0;
	sendSequenceNo_ = 0;
	timestamp_ = DUBU::GetCurrentTimeMs();
	lastPingSentTime_ = 0;
	rttMillisec_ = DEFAULT_RTT_MS;
    rudpSocket_ = nullptr;
    localWindowStart_ = 0;
    localSeqence_ = 0;

    for (Uint32 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
    {
        pendingPackets_[i] = { nullptr, 0, 0, false };
    }
}

bool DUBU::Session::RecvDispatch(Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	// 현재 시간 설정 <- 일단 수신은 된다는 뜻 그래서 갱신함. (중복, 헤더 깨짐 이런건 상관 x)
	timestamp_ = DUBU::GetCurrentTimeMs();

    // 이전 패킷 중복 넘김 (REAPET인 경우만)
    bool isRepeat = ((header->flags_ & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT);

    // 순서 체크 (recvSequenceNo_ + 1 이어야 통과)
    if (isRepeat && header->sequenceNo_ != recvSequenceNo_ + 1)
    {
        if (header->sequenceNo_ != recvSequenceNo_ + 1)
        {
            return false;
        }
        else
        {
            recvSequenceNo_ = header->sequenceNo_;
        }
    }

	// 에코 테스트
	if (header->packetCode_ == 0)
	{
		Uint32 id = header->sessionId_;
		Uint32 seq = header->sequenceNo_;
		Int32 size = header->totalSize_ - sizeof(Packet::PacketHeader);
		Uint8* ptr = reinterpret_cast<Uint8*>(buffer + sizeof(Packet::PacketHeader));

        // 에코 메시지 전달
        SendEchoMessage(ptr, size);

		std::string_view sv(reinterpret_cast<char*>(ptr), size);
		spdlog::debug("ECHO Recv : {}-{}-{}", id, seq, sv);
		return true;
	}

	// 패킷  체크
	flatbuffers::Verifier verifier(buffer + sizeof(Packet::PacketHeader), size);
	Uint8 packetCode = header->packetCode_;
	
    if (handlers_ != nullptr)
    {
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
    }
    else
    {
        spdlog::warn("Not found Packet Handler Register !!!");
    }
	return true;
}

bool DUBU::Session::RecvDispatchACK(Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);
	Uint32 ackSeq = header->sequenceNo_;
	Uint32 idx = ackSeq % DEFAULT_WINDOW_COUNT;
    
    if (!isConnect_)
    {
        // 이미 연결 끊음
        return false;
    }

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

bool DUBU::Session::RecvDispatchPong(Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	// PONG은 liveness 신호로만 사용, timestamp_만 갱신 (RTT 갱신 없음, 핸들러 실행 없음)
	timestamp_ = DUBU::GetCurrentTimeMs();

    // 카운트 갱신 가장 최근것만
    if (lastPongSeq_ == header->sequenceNo_)
    {
        AddPongCount();
    }
	return true;
}

void DUBU::Session::SendACK(Uint32 seqNo)
{
}

Uint64 DUBU::Session::GetLastPingSentTime() const
{
	return lastPingSentTime_;
}

void DUBU::Session::SetLastPingSentTime(Uint64 time)
{
	lastPingSentTime_ = time;
}

void DUBU::Session::RepeatMessage(RUDPSocket* socket, Uint64 resendDelay)
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
	OverlappedPacketBuffer* pandingbuffer = reinterpret_cast<OverlappedPacketBuffer*>(buffer);
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(pandingbuffer->buffer_);
	Uint64 timeStamp = header->timestamp_;
	Uint32 sequenceNo = header->sequenceNo_;
	bool isSent = false;

	// ACK가져올때 까지 킵
	pendingPackets_[sequenceNo % DEFAULT_WINDOW_COUNT] = { pandingbuffer, timeStamp, sequenceNo, isSent };

    // Pending 로컬 시퀀스 전진시킨다. 
    if (sequenceNo >= localSeqence_)
    {
        localSeqence_ = sequenceNo + 1;
    }
}

void DUBU::Session::Disconnect()
{
    isConnect_ = false;
    spdlog::info("Disconnect : result ping {} / {}", pongCount_, pingCount_);
}

void DUBU::Session::SendEchoMessage(Uint8* buffer, Uint16 size)
{
    if (rudpSocket_ == nullptr) return;

    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

    // 헤더 작성 (신뢰, ehco code = 0)
    header->checksum_ = 0;
    header->flags_ = Packet::PacketHeaderFlag::REPEAT;
    header->totalSize_ = static_cast <Uint16>(sizeof(Packet::PacketHeader)) + size;
    header->sessionId_ = sessionId_;
    header->sequenceNo_ = UpdateSendSequenceNo();
    header->timestamp_ = GetCurrentTimeMs();
    header->packetCode_ = 0;

    // 메시지 복사
    std::memcpy(opb->buffer_ + sizeof(Packet::PacketHeader), buffer, size);

    // 사이즈 지정
    opb->size_ = header->totalSize_;

    Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
    header->checksum_ = checksum;
    rudpSocket_->SendToReliable(GetSockAddr(), opb);
    
    // pending 전송될 때까지 대기
    AddPendingPacket(opb->buffer_, opb->size_);
}
