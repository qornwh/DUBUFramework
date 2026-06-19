#include "Client.h"
#include "RUDPSocket.h"
#include "BufferManager.h"
#include "../extra/base_flatbuffer_generated.h"

DUBU::Client::Client(const String& serverIP, Uint16 serverPort, const Map<Uint8, Packet::PacketHandler>* handlers) :
    handlers_(handlers), clientId_(0), recvSequenceNo_(0), sendSequenceNo_(0), timestamp_(0), rttMillisec_(g_defaultRttMs), isConnect_(false)
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
        timestamp_ = DUBU::GetCurrentTimeMs();
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

Uint32 DUBU::Client::UpdateSendSequenceNo()
{
    return ++sendSequenceNo_;
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

void DUBU::Client::SendEchoMessage()
{
	OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

	// ECHO 메시지
	String str = "ECHO TEST !!!";

	// 헤더 작성
	header->checksum_ = 0;
	header->flags_ = Packet::PacketHeaderFlag::REPEAT;
	header->totalSize_ = static_cast<Uint16>(sizeof(Packet::PacketHeader) + str.size());
	header->sessionId_ = clientId_;
	header->sequenceNo_ = UpdateSendSequenceNo();
	header->timestamp_ = GetCurrentTimeMs();
    header->packetCode_ = 0;

    // 메시지 복사
    std::memcpy(opb->buffer_ + sizeof(Packet::PacketHeader), str.c_str(), str.size());

	// 전체 패킷 사이즈 설정
	opb->size_ = header->totalSize_;

	// crc32 암호화
	Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
	header->checksum_ = checksum;
	rudpSocket_->SendToReliable(rudpSocket_->GetSockAddr(), opb);
    AddPendingPacket(opb, opb->size_);
}

Uint32 DUBU::Client::GetClientId() const
{
	return clientId_;
}

bool DUBU::Client::RecvDispatch(Uint8* buffer, Uint16 size)
{
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

    // 현재 시간 설정 <- 일단 수신은 된다는 뜻 그래서 갱신함. (중복, 헤더 깨짐 이런건 상관 x)
    timestamp_ = DUBU::GetCurrentTimeMs();

    // 이전 패킷 중복 넘김 (REAPET인 경우만)
    bool isRepeat = ((header->flags_ & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT);

    // 순서 체크 (recvSequenceNo_ + 1 이어야 통과)
    if (isRepeat)
    {
        if (header->sequenceNo_ <= recvSequenceNo_ && header->sequenceNo_ + DEFAULT_WINDOW_COUNT > recvSequenceNo_)
        {
            // 수신측에 recv받고 ack를 못받은 상태에서는 다시 ack를 넘겨줘야 된다 (일단 이전 DEFAULT_WINDOW_COUNT개까지 적용 시킨다. 파싱 필요x 이미 함)
            return true;
        }

        if (header->sequenceNo_ != recvSequenceNo_ + 1)
        {
            return false;
        }
        else
        {
            recvSequenceNo_ = header->sequenceNo_;
        }
    }

    // 에코 메시지 수신 완료
    if (header->packetCode_ == 0)
    {
        Uint32 id = header->sessionId_;
        Uint32 seq = header->sequenceNo_;
        Int32 size = header->totalSize_ - sizeof(Packet::PacketHeader);
        Uint8* ptr = reinterpret_cast<Uint8*>(buffer + sizeof(Packet::PacketHeader));

        std::string_view sv(reinterpret_cast<char*>(ptr), size);
        spdlog::info("ECHO Recv Client : {}-{}-{}", id, seq, sv);
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
        it->second.handler_(nullptr, buffer, size);
    }
    else
    {
        spdlog::warn("Not found Packet Handler Register !!!");
    }
    return true;
}

bool DUBU::Client::RecvDispatchACK(Uint8* buffer, Uint16 size)
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

void DUBU::Client::RepeatMessage(Uint64 resendDelay)
{
    Uint64 now = GetCurrentTimeMs();

    for (Uint32 i = localWindowStart_; i < localSeqence_; ++i)
    {
        PendingPacket& p = pendingPackets_[i % DEFAULT_WINDOW_COUNT];
        if (p.buffer != nullptr && now - p.timeStamp >= resendDelay)
        {
            rudpSocket_->SendToRepeat(rudpSocket_->GetSockAddr(), p.buffer);
            p.timeStamp = now;
        }
    }
}

void DUBU::Client::AddPendingPacket(OverlappedPacketBuffer* opb, Uint16 size)
{
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);
    Uint64 timeStamp = header->timestamp_;
    Uint32 sequenceNo = header->sequenceNo_;
    bool isSent = false;

    // ACK가져올때 까지 킵
    pendingPackets_[sequenceNo % DEFAULT_WINDOW_COUNT] = { opb, timeStamp, sequenceNo, isSent };

    // Pending 로컬 시퀀스 전진시킨다. 
    if (sequenceNo >= localSeqence_)
    {
        localSeqence_ = sequenceNo + 1;
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
    header->timestamp_ = GetCurrentTimeMs();
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
    Uint64 now = GetCurrentTimeMs();

    // 재전송로직 실행
    Uint64 delayTime = now - timestamp_;
    if (delayTime > ClientTimeout)
    {
        // 끊김 감지
        Disconnect();
    }

    // 재전송, 왕복시간은 * 2 + g_defaultRttMsDelay
    RepeatMessage(rttMillisec_ * 2 + g_defaultRttMsDelay);
}

Uint32 DUBU::Client::GetRecvSequenceNo() const
{
    return recvSequenceNo_;
}
