#include "BufferManager.h"
#include "Pool.h"

DUBU::PacketManager::~PacketManager()
{
	while (!list_.empty())
	{
		OverlappedPacketBuffer* ptr = list_.back();
		DUBU::Push<OverlappedPacketBuffer*>(ptr);
		list_.pop_back();
	}
}

void DUBU::PacketManager::Initialize()
{
	list_.reserve(baseSize_);
	for (int i = 0; i < baseSize_; ++i)
	{
		OverlappedPacketBuffer* ptr = DUBU::Pop<OverlappedPacketBuffer*>();
		ptr->pos_ = ptr->buffer_;
		ptr->size_ = sizeof(OverlappedPacketBuffer);
		OverlappedObj* ptr2 = static_cast<OverlappedObj*>(ptr);
		ptr2->Initialize();
		list_.push_back(ptr);
	}
}

DUBU::OverlappedPacketBuffer* DUBU::PacketManager::PopPacketBuffer()
{
	OverlappedPacketBuffer* ptr = list_.back();
	list_.pop_back();
	useList_.insert(ptr);
	return ptr;
}

void DUBU::PacketManager::PushPacketBuffer(OverlappedPacketBuffer* ptr)
{
	list_.push_back(ptr);
	useList_.erase(ptr);
}

const Set<DUBU::OverlappedPacketBuffer*>& DUBU::PacketManager::GetUseList()
{
	return useList_;
}
