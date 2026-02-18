#include "Server.h"
#include "RUDPSocket.h"
#include "BufferManager.h"

DUBU::Server::Server() : isRunning_(false), sessionManager_(SessionManager{})
{
	rudpServer_ = std::make_shared<RUDPSocket>();
}

DUBU::Server::~Server()
{
	rudpServer_->EndServer();
	PacketManager::GetInstance().Release();
}

void DUBU::Server::Initialize()
{
	rudpServer_->StartServer();
}

void DUBU::Server::Run()
{
	while (isRunning_)
	{
		assert(rudpServer_ != nullptr);
		LPOVERLAPPED ptr = nullptr;
		Int32 size = rudpServer_->Dispatch(&ptr);

		// 이때 다른 작업도 괞찮아 보임
		if (ptr == nullptr) 
			continue;

		OverlappedObj* ptr2 = reinterpret_cast<OverlappedObj*>(ptr);
		if ((ptr2->type_ & OverlappedObjType::RECVEFROM) == OverlappedObjType::RECVEFROM)
			rudpServer_->RecvFromComplete(ptr, size);
		else if ((ptr2->type_ & OverlappedObjType::SENDTO) == OverlappedObjType::SENDTO)
			rudpServer_->SendToComplete(ptr, size);
	}
}

void DUBU::Server::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);
}

void DUBU::Server::OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);
}
