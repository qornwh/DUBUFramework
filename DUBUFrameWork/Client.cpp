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

void DUBU::Client::SendTo(Uint8* buffer, Uint16 size)
{
	rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), buffer, size);
}

void DUBU::Client::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* buffer, Uint16 size)
{
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);
	if (header->flags_ == Packet::PacketHeaderFlag::SESSION)
	{
		clientId_ = header->sessionId_;   // 서버가 발급한 ID 저장
	}
	else
	{

	}
}

void DUBU::Client::OnSendTo(const SOCKADDR_IN& addr, Uint8* buffer, Uint16 size)
{
}

void DUBU::Client::ConnectMessage()
{
	OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
	opb->SetType(Packet::PacketHeaderFlag::SESSION);
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
	SendTo(opb->buffer_, opb->size_);
}

Uint32 DUBU::Client::GetClientId() const
{
	return clientId_;
}
