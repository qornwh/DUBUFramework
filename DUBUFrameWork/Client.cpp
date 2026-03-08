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

bool DUBU::Client::Dispatch()
{
	LPOVERLAPPED ptr = nullptr;
	Int32 size = rudpSocket_->Dispatch(&ptr);

	if (ptr == nullptr)
		return false;

	OverlappedObj* ptr2 = reinterpret_cast<OverlappedObj*>(ptr);
	if ((ptr2->type_ & OverlappedObjType::RECVEFROM) == OverlappedObjType::RECVEFROM)
		rudpSocket_->RecvFromComplete(ptr, size);
	else if ((ptr2->type_ & OverlappedObjType::SENDTO) == OverlappedObjType::SENDTO)
		rudpSocket_->SendToComplete(ptr, size);
	return true;
}

void DUBU::Client::SendTo(Uint8* ptr, Uint16 size)
{
	rudpSocket_->SendTo(rudpSocket_->GetSockAddr(), ptr, size);
}

void DUBU::Client::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
}

void DUBU::Client::OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
}
