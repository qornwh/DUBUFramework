#include "Client.h"
#include "RUDPSocket.h"
#include "Packet.h"
#include "BufferManager.h"

DUBU::Client::Client(const String& serverIP, Uint16 serverPort)
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
	// sessionId = 0, flag = SESSION 인 패킷 전송
	// 추가로 5~10번 커넥션 요청해도 안오면 연결실패 추가
	if (rudpSocket_.get() != nullptr)
	{
		ConnectMessage();
	}
	else
	{

	}
}

void DUBU::Client::Disconnect()
{
}

bool DUBU::Client::Dispatch()
{
	LPOVERLAPPED ptr = nullptr;
	Int32 size = rudpSocket_->Dispatch(&ptr);

	if (ptr == nullptr)
		return false;

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
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	auto buffer = opbPtr->buffer_;
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	auto sessionId = header->sessionId_;
	auto flag = header->flags_;

	bool result = false;
	if (flag == Packet::PacketHeaderFlag::SESSION)
	{
        // 서버가 발급한 ID 저장
		clientId_ = header->sessionId_;
	}
	else if (flag == Packet::PacketHeaderFlag::PING)
	{
        // PONG 메시지 전달
        RepeatPongMessage(buffer, size);
	}
    else if (flag == Packet::PacketHeaderFlag::DISCONNECT)
    {
        // 연결 해제
        Uint32 disconnectSeq = header->sequenceNo_;
        DisconnectMessage(disconnectSeq);
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
	rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb->buffer_, opb->size_);
}

void DUBU::Client::DisconnectMessage(Uint32 disconnectSeq)
{
    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

    // 헤더 작성
    header->checksum_ = 0;
    header->flags_ = Packet::PacketHeaderFlag::DISCONNECT;
    header->totalSize_ = sizeof(Packet::PacketHeader);
    header->sessionId_ = clientId_;
    header->sequenceNo_ = disconnectSeq;
    header->timestamp_ = 0;

    // 전체 패킷 사이즈 설정
    opb->size_ = header->totalSize_;

    // crc32 암호화
    Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
    header->checksum_ = checksum;
    rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb->buffer_, opb->size_);
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
	header->sequenceNo_ = 0;
	header->timestamp_ = 0;
	// ECHO 패킷 코드
	header->packetCode_ = 0;

	// 전체 패킷 사이즈 설정
	opb->size_ = header->totalSize_;
	// 타입 설정
	opb->SetType(OverlappedObjType::RELIABLE | OverlappedObjType::SENDTO);

	// crc32 암호화
	Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
	header->checksum_ = checksum;
	rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb->buffer_, opb->size_);
}

Uint32 DUBU::Client::GetClientId() const
{
	return clientId_;
}

void DUBU::Client::RepeatPongMessage(Uint8* ptr, Uint16 size)
{
    // 기존 버퍼 헤더
    Packet::PacketHeader* header_org = reinterpret_cast<Packet::PacketHeader*>(ptr);

    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);
    auto sequenceNo = header_org->sequenceNo_;

    // 최근 ping메시지 수신확인
    if (sequenceNo >= lastPongSeq_)
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
        rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb->buffer_, opb->size_);

        spdlog::info("PONG SeqNo {}", lastPongSeq_);
    }
}
