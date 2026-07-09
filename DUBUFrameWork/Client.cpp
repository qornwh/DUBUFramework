#include "Client.h"
#include "RUDPSocket.h"
#include "BufferManager.h"
#include "Subheader.h"
#include "../extra/base_flatbuffer_generated.h"

DUBU::Client::Client(const String& serverIP, Uint16 serverPort, const Map<Uint8, Packet::PacketHandler>* handlers) :
    handlers_(handlers), clientId_(0), timestamp_(0), rttMillisec_(g_defaultRttMs), isConnect_(false)
{
	rudpSocket_ = std::make_shared<RUDPSocket>();
	rudpSocket_->SetHandler(this);
	rudpSocket_->StartClient(serverIP, serverPort);
	rudpSocket_->RecvFrom();
}

DUBU::Client::~Client()
{
	rudpSocket_->EndClient();
}

void DUBU::Client::Connect()
{
	// clientId = 0, flag = SESSION 인 패킷 전송
	// 추가로 5~10번 커넥션 요청해도 안오면 연결실패 추가
	if (rudpSocket_.get() != nullptr)
	{
		ConnectMessage();
        timestamp_ = DUBU::GetRelativeTimeMs();
	}
	else
	{
        spdlog::error("Not Connect RUDPSocket !!!");
        assert(-1);
	}
}

void DUBU::Client::ConnectTimes(const Uint32 count)
{
    // 기본값 5회 연결
    while (!isConnect_)
    {
        Connect();
        Sleep(500);
        Dispatch();
    }

    if (!isConnect_)
    {
        spdlog::error("Not Connect Server !!!");
    }
}

void DUBU::Client::Disconnect()
{
    DisconnectMessage();
    isConnect_ = false;
}

bool DUBU::Client::Dispatch()
{
	LPOVERLAPPED ptr = nullptr;
	Int32 size = rudpSocket_->Dispatch(&ptr);

    if (ptr == nullptr)
    {
        // 일단 비어있으면 체크한다.
        CheckPending();
		return false;
    }

	OverlappedObj* ptr2 = reinterpret_cast<OverlappedObj*>(ptr);
	if ((ptr2->type_ & OverlappedObjType::RECVEFROM) == OverlappedObjType::RECVEFROM)
	{
		rudpSocket_->RecvFromComplete(ptr, size);
	}
	else if ((ptr2->type_ & OverlappedObjType::SENDTO) == OverlappedObjType::SENDTO)
	{
		rudpSocket_->SendToComplete(ptr, size);
	}
	return true;
}

void DUBU::Client::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opb = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	auto buffer = opb->buffer_;
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

    auto flag = header->flags_;
    auto seqNo = header->sequenceNo_;

	bool result = false;

    // 수신 시간 갱신
    timestamp_ = DUBU::GetRelativeTimeMs();

	if (flag == Packet::PacketHeaderFlag::SESSION)
	{
        // 서버가 발급한 ID 저장
		clientId_ = header->sessionId_;
        isConnect_ = true;
	}
	else if (flag == Packet::PacketHeaderFlag::PING)
	{
        // PONG 메시지 전달
        RepeatPongMessage(buffer, size);
	}
    else if (flag == Packet::PacketHeaderFlag::DISCONNECT)
    {
        // 연결 해제
        Disconnect();
    }
    else if (flag == Packet::PacketHeaderFlag::NONE)
    {
        // NONE 일때는 패킷을 정상 수신하여 처리 ACK 안보냄
        result = RecvDispatch(buffer, size);
    }
    else if ((flag & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT)
    {
        // ACK 일때는 내가 보낸(클라) 결과를 받고 다시 보내왔다는 뜻이다.
        result = RecvDispatch(buffer, size);
        if (result)
        {
            // ACK 전달
            SendAck(seqNo);
        }
    }
    else if ((flag & Packet::PacketHeaderFlag::ACK) == Packet::PacketHeaderFlag::ACK)
    {
        // ACK 수신 pendingpacket 지움
        RecvDispatchACK(buffer, size);
    }
}

void DUBU::Client::OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
}

void DUBU::Client::ConnectMessage()
{
    if (connNo_ >= DEFAULT_REQUEST_CONNECT_NO)
    {
        // 연결 실패
        spdlog::error("Server Not Connect, Request Count : {}", connNo_);
        return;
    }
    connNo_++;

	OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

	// 헤더 작성
	header->checksum_ = 0;
	header->flags_ = Packet::PacketHeaderFlag::SESSION;
	header->totalSize_ = sizeof(Packet::PacketHeader);
	header->sessionId_ = 0;
	header->sequenceNo_ = 0;
	header->timestamp_ = 0;

	// 전체 패킷 사이즈 설정
	opb->size_ = header->totalSize_;

	// crc32 암호화
	Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
	header->checksum_ = checksum;
	rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb);
}

void DUBU::Client::DisconnectMessage()
{
    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

    // 헤더 작성
    header->checksum_ = 0;
    header->flags_ = Packet::PacketHeaderFlag::DISCONNECT;
    header->totalSize_ = sizeof(Packet::PacketHeader);
    header->sessionId_ = clientId_;
    header->sequenceNo_ = 0;
    header->timestamp_ = 0;

    // 전체 패킷 사이즈 설정
    opb->size_ = header->totalSize_;

    // crc32 암호화
    Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
    header->checksum_ = checksum;

    // 1회만 보냄 굳이 재전송 필요 없음(1분동안 못받는 경우 or Disconnect수신시 이미 끊길 확률 높음)
    rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb);
}

Uint32 DUBU::Client::GetClientId() const
{
    return clientId_;
}

bool DUBU::Client::RecvDispatch(Uint8* buffer, Uint16 size)
{
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

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
        PacketParse(buffer, size);
        return true;
    }

    return true;
}

bool DUBU::Client::RecvDispatchACK(Uint8* buffer, Uint16 size)
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

void DUBU::Client::RepeatMessageAll(Uint32 resendDelay)
{
    Uint32 now = GetRelativeTimeMs();

    if (rpsNo_.IsRepeat())
    {
        RepeatMessage(resendDelay, rpsNo_, now);
    }

    for (Uint32 channelID = 0; channelID <= (g_channelMask); ++channelID)
    {
        ReliablePacketState& rps = cacheAlreadyPackets_[channelID].reliablePacketState;
        if (rps.IsRepeat())
        {
            RepeatMessage(resendDelay, rps, now);
        }
    }
}

void DUBU::Client::PacketParse(Uint8* buffer, Uint16 size)
{
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

    // 에코 메시지 수신 완료
    if (header->packetCode_ == 0)
    {
        Uint32 id = header->sessionId_;
        Uint32 seq = header->sequenceNo_;
        Int32 size = header->totalSize_ - sizeof(Packet::PacketHeader);
        Uint8* ptr = reinterpret_cast<Uint8*>(buffer + sizeof(Packet::PacketHeader));

        std::string_view sv(reinterpret_cast<char*>(ptr), size);
        spdlog::info("ECHO Recv Client : {}-{}-{}", id, seq, sv);
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
            it->second.handler2_(nullptr, buffer, size, shBuffer, shType);
        }
        else
        {
            it->second.handler_(nullptr, buffer, size);
        }
    }
    else
    {
        spdlog::warn("Not found Packet Handler Register !!!");
    }
}

void DUBU::Client::SendAck(Uint32 seqNo)
{
    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

    // 헤더 작성 (header-only, 비신뢰)
    header->checksum_ = 0;
    header->flags_ = Packet::PacketHeaderFlag::ACK;
    header->totalSize_ = sizeof(Packet::PacketHeader);
    header->sessionId_ = clientId_;
    header->sequenceNo_ = seqNo;
    header->timestamp_ = GetRelativeTimeMs();
    header->packetCode_ = 0;

    opb->size_ = header->totalSize_;

    Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
    header->checksum_ = checksum;
    rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb);
}

void DUBU::Client::RepeatPongMessage(Uint8* ptr, Uint16 size)
{
    // 기존 버퍼 헤더
    Packet::PacketHeader* header_org = reinterpret_cast<Packet::PacketHeader*>(ptr);

    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);
    auto sequenceNo = header_org->sequenceNo_;

    // 최근 ping메시지 수신확인
    if (lastPongSeq_ <= sequenceNo)
    {
        lastPongSeq_ = sequenceNo;

        // 헤더 작성
        header->checksum_ = 0;
        header->flags_ = Packet::PacketHeaderFlag::PONG;
        header->totalSize_ = sizeof(Packet::PacketHeader);
        header->sessionId_ = clientId_;
        header->sequenceNo_ = sequenceNo;
        header->timestamp_ = 0;

        // 전체 패킷 사이즈 설정
        opb->size_ = header->totalSize_;
        // 타입 설정
        opb->SetType(OverlappedObjType::SENDTO);

        // crc32 암호화
        Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
        header->checksum_ = checksum;
        rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb);

        spdlog::info("PONG SeqNo {}", lastPongSeq_);
    }
}

void DUBU::Client::CheckPending()
{
    // 시간체크
    Uint32 now = GetRelativeTimeMs();

    // 재전송로직 실행
    Uint32 delayTime = now - timestamp_;
    if (delayTime > ClientTimeout)
    {
        // 끊김 감지
        Disconnect();
    }

    // 재전송, 왕복시간은 * 2 + g_defaultRttMsDelay
    RepeatMessageAll(rttMillisec_ * 2 + g_defaultRttMsDelay);
}

void DUBU::Client::SendEchoMessage()
{
    String str = "ECHO TEST !!!";
    char* ptr = const_cast<char*>(str.c_str());
    SendPacket(reinterpret_cast<Uint8*>(ptr), 0, static_cast<Uint16>(str.size()), Packet::PacketOpctions{true, true, 0});
}

void DUBU::Client::SendPacket(Uint8* buffer, Uint8 code, Uint16 size, const Packet::PacketOpctions& opt, const Uint8* subHeader, Uint16 subHeaderSize)
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
    header->sessionId_ = clientId_;
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
        rudpSocket_->SendToReliable(rudpSocket_->GetSockAddr(), opb);

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
        rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb);
    }
}

void DUBU::Client::RepeatMessage(Uint32 resendDelay, ReliablePacketState& rps, Uint32 now)
{
    Uint32 current = rps.localWindowStart_;
    while (current != rps.localSeqence_)
    {
        PendingPacket& p = rps.pendingPackets_[current % DEFAULT_WINDOW_COUNT];
        if (p.buffer != nullptr && now - p.timeStamp >= resendDelay)
        {
            rudpSocket_->SendToRepeat(rudpSocket_->GetSockAddr(), p.buffer);
            p.timeStamp = now;
        }
        ++current;
    }
}
