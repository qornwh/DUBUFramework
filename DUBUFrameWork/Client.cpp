#include "Client.h"
#include "RUDPSocket.h"
#include "Packet.h"
#include "BufferManager.h"

DUBU::Client::Client(const String& serverIP, Uint16 serverPort)
{
	rudpSocket_ = std::make_shared<RUDPSocket>();
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
		OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
		opb->SetType(Packet::PacketHeaderFlag::SESSION);
		rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), opb->buffer_, opb->size_);
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
