#include "Session.h"
#include "RUDPSocket.h"
#include "BufferManager.h"
#include "Subheader.h"
#include "../extra/base_flatbuffer_generated.h"

DUBU::Session::Session(const Map<Uint8, Packet::PacketHandler>* handlers) :
	handlers_(handlers), sessionId_(0), timestamp_(0), lastPingSentTime_(0), addr_(), rttMillisec_(g_defaultRttMs), isConnect_(false)
{
}

DUBU::Session::~Session()
{
    if (isConnect_)
    {
        Reset();
    }
}

void DUBU::Session::SetSockAddr(const SOCKADDR_IN& addr)
{
	addr_ = addr;
	isConnect_ = true;
	timestamp_ = DUBU::GetRelativeTimeMs();
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

void DUBU::Session::SetTimestamp(const Uint32 time)
{
	timestamp_ = time;
}

Uint32 DUBU::Session::GetSessionId() const
{
	return sessionId_;
}

Uint32 DUBU::Session::GetRetryCount() const
{
	return retryCount_;
}

Uint32 DUBU::Session::GetRttMillisec() const
{
	return rttMillisec_;
}

Uint32 DUBU::Session::GetTimestamp() const
{
	return timestamp_;
}

Bool DUBU::Session::IsConnection() const
{
	return isConnect_;
}

void DUBU::Session::Reset()
{
    if (isConnect_ == true)
    {
        isConnect_ = false;
    }

	// 세션 초기화
	sessionId_ = 0;
	timestamp_ = DUBU::GetRelativeTimeMs();
	lastPingSentTime_ = 0;
	rttMillisec_ = g_defaultRttMs;
    rudpSocket_ = nullptr;

    for (Uint32 channelID = 0; channelID < (1 << 6); ++channelID)
    {
        CacheAlreadyPacket& cap = cacheAlreadyPackets_[channelID];
        ReliablePacketState& rps = cap.reliablePacketState;

        for (Uint32 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
        {
            CachePacket& cachePacket = cap.cachePackets[i];
            if (cachePacket.buffer != nullptr)
            {
                cachePacket.isKeep = false;
                cachePacket.sequenceNo = 0;
                cachePacket.timeStamp = 0;
                CachePacketManager::GetInstance().PushPacketBuffer(cachePacket.buffer);
            }
        }
    }
}

bool DUBU::Session::RecvDispatch(Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	// 현재 시간 설정 <- 일단 수신은 된다는 뜻 그래서 갱신함. (중복, 헤더 깨짐 이런건 상관 x)
	timestamp_ = DUBU::GetRelativeTimeMs();

    // 이전 패킷 중복 넘김 (REAPET인 경우만)
    bool isRepeat = ((header->flags_ & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT);

    if (isRepeat)
    {
        Uint8 channel = header->flags_ & Packet::PacketHeaderFlag::CHANNEL;
        if (channel > 0)
        {
            if (header->flags_ > 0b1111)
            {
                // 할당 불가 채널 ID.
                return false;
            }

            // 채널 ID
            Uint8 channelID = (header->flags_ & Packet::PacketHeaderFlag::CHANNELMASK) << 3;

            CacheAlreadyPacket& cap = cacheAlreadyPackets_[channelID];
            ReliablePacketState& rps = cap.reliablePacketState;

            // 순서 체크 (recvSequenceNo_ + 1 이어야 통과)
            if (header->sequenceNo_ <= rps.recvRepeatSeq_ && header->sequenceNo_ + DEFAULT_WINDOW_COUNT > rps.recvRepeatSeq_)
            {
                // 수신측에 recv받고 ack를 못받은 상태에서는 다시 ack를 넘겨줘야 된다 (일단 이전 DEFAULT_WINDOW_COUNT개까지 적용 시킨다. 파싱 필요x 이미 함)
                return true;
            }

            if (header->sequenceNo_ != rps.recvRepeatSeq_ + 1)
            {
                // 캐싱
                Uint64 del = header->sequenceNo_ - rps.recvRepeatSeq_;
                if (del > 64)
                {
                    // 최대 캐싱 가능크기는 64넘기면 캐싱안하고 넘어간다.
                    false;
                }

                if (header->sequenceNo_ > rps.lastRepeatSeq_)
                {
                    // 패킷 캐싱
                    CachePacket& cachePacket = cap.cachePackets[header->sequenceNo_ % DEFAULT_WINDOW_COUNT];
                    cachePacket.buffer = CachePacketManager::GetInstance().PopPacketBuffer();
                    cachePacket.sequenceNo = header->sequenceNo_;
                    cachePacket.timeStamp = DUBU::GetRelativeTimeMs();
                    cachePacket.isKeep = true;

                    rps.lastRepeatSeq_ = header->sequenceNo_;
                    rps.cacheRepeatCount_ &= static_cast<Uint64>(1) << (del);
                }
                return true;
            }
            else
            {
                CacheAlreadyPacket& cap = cacheAlreadyPackets_[channelID];
                ReliablePacketState& rps = cap.reliablePacketState;

                // 현재꺼는 실행
                PacketParse(buffer, size);
                rps.cacheRepeatCount_ <<= 1;

                // 미리 수신된 패킷 있으면 실행해 준다.
                for (Uint64 i = rps.recvRepeatSeq_ + 1; i < rps.lastRepeatSeq_; ++i)
                {
                    CachePacket& cachePacket = cap.cachePackets[i % DEFAULT_WINDOW_COUNT];
                    if (!cachePacket.isKeep)
                    {
                        break;
                    }

                    // 실행 후 반환한다.
                    PacketParse(cachePacket.buffer->buffer_, cachePacket.buffer->size_);
                    cachePacket.isKeep = false;
                    CachePacketManager::GetInstance().PushPacketBuffer(cachePacket.buffer);
                }
                return true;
            }
        }
        else
        {
            // 순서 상관 x
            if (header->sequenceNo_ <= rpsNo_.recvRepeatSeq_)
            {
                return true;
            }
            else if (header->sequenceNo_ > rpsNo_.recvRepeatSeq_ + 1)
            {
                // 현재 순서가 아닌 패킷인 경우
                Uint64 del64 = header->sequenceNo_ - rpsNo_.recvRepeatSeq_;
                if (del64 > 63)
                {
                    // 63초과한 미래이면 그냥 패스 - 재전송 하라고 한다.
                    return false;
                }

                // 마지막 갱신
                if (header->sequenceNo_ > rpsNo_.lastRepeatSeq_)
                {
                    header->sequenceNo_ = rpsNo_.lastRepeatSeq_;
                }

                // 실행여부 확인후 넘긴다.
                Uint8 del = static_cast<Uint8>(1 << del64);
                if ((rpsNo_.cacheRepeatCount_ & del) != del)
                {
                    rpsNo_.cacheRepeatCount_ &= del;
                    PacketParse(buffer, size);
                }
                return true;
            }
            else
            { 
                rpsNo_.recvRepeatSeq_ = header->sequenceNo_;
                rpsNo_.cacheRepeatCount_ <<= 1;
                PacketParse(buffer, size);

                // 이미 처리된 경우 1씩 땡긴다.
                while ((rpsNo_.cacheRepeatCount_ & 1) == 1)
                {
                    rpsNo_.cacheRepeatCount_ <<= 1;
                    ++rpsNo_.recvRepeatSeq_;
                }
                return true;
            }
        }
    }
    else
    {
        // 재전송 패킷이 아닌경우.
    }

    return false;
}

bool DUBU::Session::RecvDispatchACK(Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);
	Uint32 ackSeq = header->sequenceNo_;
	Uint32 idx = ackSeq % DEFAULT_WINDOW_COUNT;
    Uint8 ischannel = (header->flags_ & Packet::PacketHeaderFlag::CHANNEL);
    
    if (!isConnect_)
    {
        // 이미 연결 끊음
        return false;
    }

    if (ischannel)
    {
        Uint8 channel = (header->flags_ & Packet::PacketHeaderFlag::CHANNELMASK) << 3;
        cacheAlreadyPackets_[channel].reliablePacketState.AckProcess(ackSeq, rttMillisec_);
    }
    else
    {
        rpsNo_.AckProcess(ackSeq, rttMillisec_);
    }

	return true;
}

bool DUBU::Session::RecvDispatchPong(Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	// PONG은 liveness 신호로만 사용, timestamp_만 갱신 (RTT 갱신 없음, 핸들러 실행 없음)
	timestamp_ = DUBU::GetRelativeTimeMs();

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

Uint32 DUBU::Session::GetLastPingSentTime() const
{
	return lastPingSentTime_;
}

void DUBU::Session::SetLastPingSentTime(Uint32 time)
{
	lastPingSentTime_ = time;
}

void DUBU::Session::RepeatMessageAll(RUDPSocket* socket, Uint32 resendDelay)
{
	Uint32 now = GetRelativeTimeMs();

    if (rpsNo_.IsRepeat())
    {
        RepeatMessage(socket, resendDelay, rpsNo_, now);
    }

    for (Uint32 channelID = 0; channelID < (1 << 6); ++channelID)
    {
        ReliablePacketState& rps = cacheAlreadyPackets_[channelID].reliablePacketState;
        if (rps.IsRepeat())
        {
            RepeatMessage(socket, resendDelay, rps, now);
        }
    }
}

void DUBU::Session::PacketParse(Uint8* buffer, Uint16 size)
{
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

    // 에코 테스트
    if (header->packetCode_ == 0)
    {
        Uint32 id = header->sessionId_;
        Uint32 seq = header->sequenceNo_;
        Int32 size = header->totalSize_ - sizeof(Packet::PacketHeader);
        Uint8* ptr = reinterpret_cast<Uint8*>(buffer + sizeof(Packet::PacketHeader));

        // 에코 메시지 전달
        //SendEchoMessage(ptr, size);

        std::string_view sv(reinterpret_cast<char*>(ptr), size);
        spdlog::info("ECHO Recv Server : {}-{}-{}", id, seq, sv);
        return;
    }

    // 패킷  체크
    Uint8 shType = header->packetCode_ >> 5;
    Uint8 packetCode = header->packetCode_ & 0b00011111;
    Uint32 offset = sizeof(Packet::PacketHeader);
    Uint8* shBuffer = nullptr;

    if (shType > 0)
    {
        Packet::SubheaderBase* sh = reinterpret_cast<Packet::SubheaderBase*>(buffer + sizeof(Packet::PacketHeader));
        offset += sh->GetSize();
        shBuffer = reinterpret_cast<Uint8*>(sh);
    }

    flatbuffers::Verifier verifier(buffer + offset, size);

    if (handlers_ != nullptr)
    {
        auto it = handlers_->find(packetCode);
        if (it == handlers_->end())
        {
            // 패킷코드에 대한 함수가 등록되지 않음
            spdlog::error("Not Found PacketCode : {} !!!", packetCode);
            return;
        }

        if (!it->second.verifier_(verifier))
        {
            // 패킷이 정확하지 않음
            spdlog::warn("Verfiy Failed !!!");
            return;
        }

        // 패킷별 함수 실행
        if (shType > 0 && shBuffer != nullptr)
        {
            it->second.handler2_(this, buffer, size, shBuffer, shType);
        }
        else
        {
            it->second.handler_(this, buffer, size);
        }
    }
    else
    {
        spdlog::warn("Not found Packet Handler Register !!!");
    }
}

void DUBU::Session::SetPeer(Peer& peer)
{
	peer_ = peer;
}

void DUBU::Session::Disconnect()
{
    isConnect_ = false;
#ifdef _DEBUG
    spdlog::info("Disconnect : result resent {} -- ping {} / {}", resendCount_.load(), pongCount_, pingCount_);
#endif
}

void DUBU::Session::SendPacket(Uint8* buffer, Uint8 code, Uint16 size, const Packet::PacketOpctions& opt, const Uint8* subHeader, Uint16 subHeaderSize)
{
    if (rudpSocket_ == nullptr) return;

    // 패킷 메모리 할당
    Uint32 offset = sizeof(Packet::PacketHeader);
    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

    // 패킷 헤더 옵션 설정
    header->flags_ = Packet::PacketHeaderFlag::NONE;
    if (opt.reliable_)
    {
        header->flags_ |= Packet::PacketHeaderFlag::REPEAT;
        if (opt.order_)
        {
            header->flags_ |= opt.channelID_ << 2;
            header->sequenceNo_ = cacheAlreadyPackets_[opt.channelID_].reliablePacketState.UpdateSendSequenceNo();
        }
        else
        {
            header->sequenceNo_ = rpsNo_.UpdateSendSequenceNo();
        }
    }
    else
    {
        header->sequenceNo_ = rNopsNo_.UpdateSendSequenceNo();
    }
    header->checksum_ = 0;
    header->totalSize_ = static_cast <Uint16>(sizeof(Packet::PacketHeader)) + size;
    header->sessionId_ = sessionId_;
    header->timestamp_ = GetRelativeTimeMs();
    header->packetCode_ = code;

    if (subHeader != nullptr && subHeaderSize > 0)
    {
        const Packet::SubheaderBase* sh = reinterpret_cast<const Packet::SubheaderBase*>(subHeader);
        // 서브헤더 크기만큼 복사해 준다.
        std::memcpy(opb->buffer_ + offset, subHeader, subHeaderSize);
        offset += subHeaderSize;
        // 서브헤더 코드 비트연산으로 체크 가능하도록
        header->packetCode_ |= (sh->type_ << 5);
        header->totalSize_ += subHeaderSize;
    }
    
    // 사이즈 지정
    opb->size_ = header->totalSize_;

    // 체크썸 계산
    Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
    header->checksum_ = checksum;

    if (opt.reliable_)
    {
        rudpSocket_->SendToReliable(GetSockAddr(), opb);

        // pending 전송될 때까지 대기
        if (opt.order_)
        {
            cacheAlreadyPackets_[opt.channelID_].reliablePacketState.AddPendingPacket(opb, opb->size_);
        }
        else
        {
            rpsNo_.AddPendingPacket(opb, opb->size_);
        }
    }
    else
    {
        rudpSocket_->SendTo(GetSockAddr(), opb);
    }
}

void DUBU::Session::SetAwaysConnect(Bool awaysConnect)
{
    awaysConnect_ = awaysConnect;
}

void DUBU::Session::RepeatMessage(RUDPSocket* socket, Uint32 resendDelay, ReliablePacketState& rps, Uint32 now)
{
    Uint32 current = rps.localWindowStart_;
    while (current != rps.localSeqence_)
    {
        PendingPacket& p = rps.pendingPackets_[current % DEFAULT_WINDOW_COUNT];
        if (p.buffer != nullptr && now - p.timeStamp >= resendDelay)
        {
            socket->SendToRepeat(addr_, p.buffer);
            p.timeStamp = now;
#ifdef _DEBUG
            resendCount_.fetch_add(1);
#endif
        }
        ++current;
    }
}
