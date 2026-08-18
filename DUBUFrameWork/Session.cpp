#include "Session.h"
#include "RUDPSocket.h"
#include "BufferManager.h"
#include "Subheader.h"
#include "../extra/base_flatbuffer_generated.h"
#include "Pool.h"

DUBU::Session::Session(const Map<Uint8, Packet::PacketHandler>* handlers) :
	handlers_(handlers), sessionId_(0), timestamp_(0), lastPingSentTime_(0), addr_(), rttMillisec_(g_defaultRttMs), isConnect_(false), chunckPakcetInj_{}
{
    cacheAlreadyPackets_.resize(g_channelMask + 1);
}

DUBU::Session::~Session()
{
    if (isConnect_)
    {
        Reset();
    }
    cacheAlreadyPackets_.clear();
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

    rNopsNo_.Reset();
    rpsNo_.Reset();
    for (Uint32 channelID = 0; channelID <= (g_channelMask); ++channelID)
    {
        CacheAlreadyPacket& cap = cacheAlreadyPackets_[channelID];
        ReliablePacketState& rps = cap.reliablePacketState;

        rps.Reset();
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
            // 채널 ID
            Uint8 channelID = (header->flags_ & Packet::PacketHeaderFlag::CHANNELMASK) >> 3;
            if (channelID > g_channelMask)
            {
                // 할당 불가 채널 ID.
                return false;
            }

            CacheAlreadyPacket& cap = cacheAlreadyPackets_[channelID];
            ReliablePacketState& rps = cap.reliablePacketState;

            // 수신측에 recv받고 ack를 못받은 상태에서는 다시 ack를 넘겨줘야 된다. (일단 이전 DEFAULT_WINDOW_COUNT개까지 적용 시킨다. 파싱 필요x 이미 함)
            if (rps.recvRepeatSeq_ - header->sequenceNo_ < DEFAULT_WINDOW_COUNT)
            {
                return true;
            }

            if (header->sequenceNo_ != rps.recvRepeatSeq_ + 1)
            {
                // 캐싱
                Uint32 del = header->sequenceNo_ - rps.recvRepeatSeq_;
                if (del >= DEFAULT_WINDOW_COUNT)
                {
                    // 최대 캐싱 가능크기는 64넘기면 캐싱안하고 넘어간다.
                    return false;
                }

                // 마지막 갱신
                Uint32 fwd = header->sequenceNo_ - rps.lastRepeatSeq_;
                if (fwd < DEFAULT_WINDOW_COUNT)
                {
                    rps.lastRepeatSeq_ = header->sequenceNo_;
                }

                // 패킷 캐싱
                CachePacket& cachePacket = cap.cachePackets[header->sequenceNo_ % DEFAULT_WINDOW_COUNT];
                if (cachePacket.isKeep && cachePacket.sequenceNo == header->sequenceNo_)
                {
                    // 이미 캐싱된 패킷의 재전송이라 다시 저장할 필요 없음
                    return true;
                }
                cachePacket.buffer = CachePacketManager::GetInstance().PopPacketBuffer();
                cachePacket.buffer->Copy(buffer, size);
                cachePacket.sequenceNo = header->sequenceNo_;
                cachePacket.timeStamp = DUBU::GetRelativeTimeMs();
                cachePacket.isKeep = true;

                rps.cacheRepeatCount_ |= static_cast<Uint64>(1) << (del - 1);

                return true;
            }
            else
            {
                // 현재 수신된 시퀀스 넘버 == 가장높은 last가 같은지 체크
                bool isLastUpdate = false;
                if (rps.recvRepeatSeq_ == rps.lastRepeatSeq_)
                {
                    isLastUpdate = true;
                }

                // 현재꺼는 실행
                PacketParse(buffer, size);
                rps.cacheRepeatCount_ >>= 1;
                rps.recvRepeatSeq_ = header->sequenceNo_;

                // 미리 수신된 패킷 있으면 실행해 준다.
                Uint32 idx = rps.recvRepeatSeq_;
                while (idx != rps.lastRepeatSeq_)
                {
                    ++idx;
                    CachePacket& cachePacket = cap.cachePackets[idx % DEFAULT_WINDOW_COUNT];
                    if (!cachePacket.isKeep || cachePacket.sequenceNo != idx)
                    {
                        break;
                    }

                    // 실행 후 반환한다.
                    PacketParse(cachePacket.buffer->buffer_, cachePacket.buffer->size_);
                    cachePacket.isKeep = false;
                    CachePacketManager::GetInstance().PushPacketBuffer(cachePacket.buffer);
                    ++rps.recvRepeatSeq_;
                    rps.cacheRepeatCount_ >>= 1;
                }

                if (isLastUpdate)
                {
                    rps.lastRepeatSeq_ = rps.recvRepeatSeq_;
                }
                return true;
            }
        }
        else
        {
            // 순서 상관 x인 패킷
            Uint8 chunck = header->flags_ & Packet::PacketHeaderFlag::CHUNK;

            // ACK 못받은거 처리
            if (rpsNo_.recvRepeatSeq_ - header->sequenceNo_ < DEFAULT_WINDOW_COUNT)
            {
                return true;
            }

            if (header->sequenceNo_ != rpsNo_.recvRepeatSeq_ + 1)
            {
                // 현재 순서가 아닌 패킷인 경우
                Uint32 del = header->sequenceNo_ - rpsNo_.recvRepeatSeq_;

                if (del >= DEFAULT_WINDOW_COUNT)
                {
                    // TODO : 반복될시 disconnect
                    return false;
                }

                // 마지막 갱신
                Uint32 fwd = header->sequenceNo_ - rpsNo_.lastRepeatSeq_;
                if (fwd < DEFAULT_WINDOW_COUNT)
                {
                    rpsNo_.lastRepeatSeq_ = header->sequenceNo_;
                }

                // 실행여부 확인후 넘긴다. (1차이가 나는경우는 없다, 그래서 1뺀다)
                Uint64 bit = static_cast<Uint64>(1) << (del - 1);
                if ((rpsNo_.cacheRepeatCount_ & bit) != bit)
                {
                    rpsNo_.cacheRepeatCount_ |= bit;
                    if (chunck == 0)
                    {
                        PacketParse(buffer, size);
                    }
                    else
                    {
                        RecvChunckPacket(buffer, size);
                    }
                }
                return true;
            }
            else
            { 
                // 현재 수신된 시퀀스 넘버 == 가장높은 last가 같은지 체크
                bool isLastUpdate = false; 
                if (rpsNo_.recvRepeatSeq_ == rpsNo_.lastRepeatSeq_)
                {
                    isLastUpdate = true;
                }

                rpsNo_.recvRepeatSeq_ = header->sequenceNo_;
                rpsNo_.cacheRepeatCount_ >>= 1;
                if (chunck == 0)
                {
                    PacketParse(buffer, size);
                }
                else
                {
                    RecvChunckPacket(buffer, size);
                }

                // 이미 처리된 경우 1씩 땡긴다.
                while ((rpsNo_.cacheRepeatCount_ & 1) == 1)
                {
                    rpsNo_.cacheRepeatCount_ >>= 1;
                    ++rpsNo_.recvRepeatSeq_;
                }

                if (isLastUpdate)
                {
                    rpsNo_.lastRepeatSeq_ = rpsNo_.recvRepeatSeq_;
                }

                return true;
            }
        }
    }
    else
    {
        Uint32 del = header->sequenceNo_ - rNopsNo_.recvRepeatSeq_;
        if (del == 0 || del >= NO_REPEAT_ACCEPT_RANGE)
        {
            // TODO : NO_REPEAT_ACCEPT_RANGE <- 이거 넘어가면 문제있는거다.
            return false;
        }

        // 재전송 패킷이 아닌경우.
        PacketParse(buffer, size);
        rNopsNo_.recvRepeatSeq_ = header->sequenceNo_;
        return true;
    }

    return true;
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
        Uint8 channel = (header->flags_ & Packet::PacketHeaderFlag::CHANNELMASK) >> 3;
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

void DUBU::Session::SendACK(Uint32 seqNo, const Packet::PacketOpctions& opt)
{
    // 서버 처리 내가보낸거 ACK만 보내줌
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

    for (Uint32 channelID = 0; channelID <= (g_channelMask); ++channelID)
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
        Uint32 echoSize = header->totalSize_ - sizeof(Packet::PacketHeader);
        Uint8* ptr = reinterpret_cast<Uint8*>(buffer + sizeof(Packet::PacketHeader));

        // 에코 메시지 전달
        SendEchoMessage(ptr, echoSize);

        std::string_view sv(reinterpret_cast<char*>(ptr), echoSize);
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

    flatbuffers::Verifier verifier(buffer + offset, size - offset);

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

void DUBU::Session::SendEchoMessage(Uint8* buffer, Uint16 size)
{
    Packet::PacketOpctions opt{ true, true, 0 };
    SendPacket(buffer, 0, size, opt);
}

void DUBU::Session::SendPacket(Uint8* buffer, Uint8 code, Uint16 size, const Packet::PacketOpctions& opt, const Uint8* subHeader, Uint16 subHeaderSize)
{
    if (rudpSocket_ == nullptr) return;

    Uint32 offset = sizeof(Packet::PacketHeader);

    // 크기 초과면 청크로 분할
    if (size + subHeaderSize + offset > PACKET_MAX_SIZE)
    {
        SendPacketChunk(buffer, code, size, subHeader, subHeaderSize);
        return;
    }

    // 여러 스레드가 동시에 보낼 수 있어 SendPacket만 lock
    WriteLockGuard wl(sendLock_);

    // 패킷 메모리 할당
    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

    // 패킷 헤더 옵션 설정
    header->flags_ = Packet::PacketHeaderFlag::NONE;
    if (opt.reliable_)
    {
        header->flags_ |= Packet::PacketHeaderFlag::REPEAT;
        if (opt.order_)
        {
            header->flags_ |= Packet::PacketHeaderFlag::CHANNEL;
            header->flags_ |= opt.channelID_ << 3;
            header->sequenceNo_ = cacheAlreadyPackets_[opt.channelID_].reliablePacketState.UpdateSendSequenceNo();
        }
        else if (opt.isChunck_)
        {
            header->flags_ |= Packet::PacketHeaderFlag::CHUNK;
            header->chunkInfo_.flag_ = opt.chunckFlag_;
            header->chunkInfo_.size_ = opt.chunckTotal_;
            header->sequenceNo_ = rpsNo_.UpdateSendSequenceNo();
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

    // 메시지 복사
    std::memcpy(opb->buffer_ + offset, buffer, size);
    
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

void DUBU::Session::RecvChunckPacket(Uint8* buffer, Uint16 size)
{
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

    // 청크 패킷 처리
    Uint8 chunck = header->flags_ & Packet::PacketHeaderFlag::CHUNK;
    if (chunck == Packet::PacketHeaderFlag::CHUNK)
    {
        Uint32 seqNo = header->sequenceNo_;
        // 일단 chunk는 채널 사용x : 현재 시스템상 채널은 순서대로 무조건 받는다. 그럼으로 제외
        Packet::ChunkInfo chunckInfo = header->chunkInfo_;
        ChunkPacket& chunckPacket = chunckPakcetInj_.Update(seqNo, chunckInfo.flag_);
        CachePacketBuffer* cachePacketBuffer = CachePacketManager::GetInstance().PopPacketBuffer();
        cachePacketBuffer->Copy(buffer, size);
        chunckPacket.SetBuffer(chunckInfo.flag_, cachePacketBuffer);
        chunckPacket.size_ = chunckInfo.size_;

        if (chunckPacket.IsPull())
        {
            Uint16 headerSize = sizeof(Packet::PacketHeader);

            // 서브헤더 패킷  체크
            Uint8 shType = header->packetCode_ >> 5;
            if (shType > 0)
            {
                Packet::SubheaderBase* sh = reinterpret_cast<Packet::SubheaderBase*>(buffer + headerSize);
                headerSize += sh->GetSize();
            }

            Uint16 chunckOffset = 0;
            Uint8* ptr = DUBU::PopBig(chunckPacket.size_ + headerSize);
            for (Int32 i = 0; i < chunckPacket.count_; ++i)
            {
                CachePacketBuffer* cpb = chunckPacket.buffers_[i];
                if (i > 0)
                {
                    memcpy(ptr + chunckOffset, cpb->buffer_ + headerSize, cpb->size_ - headerSize);
                    chunckOffset += (cpb->size_ - headerSize);
                }
                else
                {
                    // 첫번째 헤더는 그대로 가져간다.ㅇ
                    memcpy(ptr + chunckOffset, cpb->buffer_, cpb->size_);
                    chunckOffset += cpb->size_;
                }
                CachePacketManager::GetInstance().PushPacketBuffer(cpb);
            }
            // 청크는 이때 파싱한다. 
            // 이유는 모든 청크를 수집해서 parse하고
            // 이후에는 동적할당 해제 필수.. 릭 방지
            PacketParse(ptr, chunckOffset); // 1개의 헤더가 더해진 사이즈
            DUBU::PushBig(ptr);
            chunckPakcetInj_.Remove(seqNo, chunckInfo.flag_);
        }
    }
}

void DUBU::Session::SendPacketChunk(Uint8* buffer, Uint8 code, Uint16 size, const Uint8* subHeader, Uint16 subHeaderSize)
{
    Uint16 chunckSize = PACKET_MAX_SIZE - sizeof(Packet::PacketHeader) - subHeaderSize;
    Uint16 count = (size / chunckSize);
    if (size % chunckSize > 0)
    {
        ++count;
    }

    Uint32 offset = 0;
    Packet::PacketOpctions opt;
    opt.reliable_ = true;
    opt.isChunck_ = true;
    opt.chunckTotal_ = size;
    Uint16 lastBit = ~0;
    for (Uint32 i = 0; i < count; ++i)
    {
        Uint16 sendSize = chunckSize;

        // 마지막 처리
        if (i == count - 1)
        {
            sendSize = size % chunckSize;
            opt.chunckFlag_ = lastBit;
        }
        else
        {
            opt.chunckFlag_ = 1 << i;
            // 마지막 체크용 하나씩 끈다.
            lastBit <<= 1;
        }

        SendPacket(buffer + offset, code, sendSize, opt, subHeader, subHeaderSize);
        offset += chunckSize;
    }
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
