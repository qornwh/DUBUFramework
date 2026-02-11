#include "Service.h"
#include "RUDPSocket.h"
#include "BufferManager.h"

DUBU::Service::Service() : isRunning_(false)
{
	rudpServer_ = std::make_shared<RUDPSocket>();
}

DUBU::Service::~Service()
{
	rudpServer_->EndServer();
	PacketManager::GetInstance().Release();
}

void DUBU::Service::Initialize()
{
	rudpServer_->StartServer();
}

void DUBU::Service::Run()
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
