#include "BufferManager.h"
#include "Pool.h"

DUBU::PacketManager::~PacketManager()
{
	Release();
}

void DUBU::PacketManager::Initialize()
{
	list_.reserve(baseSize_);
	for (int i = 0; i < baseSize_; ++i)
	{
		OverlappedPacketBuffer* ptr = DUBU::Pop<OverlappedPacketBuffer>();
		ptr->pos_ = ptr->buffer_;
		ptr->size_ = sizeof(OverlappedPacketBuffer);
		OverlappedObj* ptr2 = static_cast<OverlappedObj*>(ptr);
		ptr2->Initialize();
		list_.push_back(ptr);
	}
}

void DUBU::PacketManager::Release()
{
	WriteLockGuard wl(lk_);
	while (!list_.empty())
	{
		OverlappedPacketBuffer* ptr = list_.back();
		DUBU::Push<OverlappedPacketBuffer>(ptr);
		list_.pop_back();
	}
}

DUBU::OverlappedPacketBuffer* DUBU::PacketManager::PopPacketBuffer()
{
	WriteLockGuard wl(lk_);
	if (!list_.empty())
	{
		OverlappedPacketBuffer* ptr = list_.back();
		list_.pop_back();
		useList_.insert(ptr);
		return ptr;
	}

	OverlappedPacketBuffer* ptr = DUBU::Pop<OverlappedPacketBuffer>();
	ptr->pos_ = ptr->buffer_;
	ptr->size_ = sizeof(OverlappedPacketBuffer);
	OverlappedObj* ptr2 = static_cast<OverlappedObj*>(ptr);
	ptr2->Initialize();
	return ptr;
}

void DUBU::PacketManager::PushPacketBuffer(OverlappedPacketBuffer* ptr)
{
	WriteLockGuard wl(lk_);
	list_.push_back(ptr);
	useList_.erase(ptr);
}

const Set<DUBU::OverlappedPacketBuffer*>& DUBU::PacketManager::GetUseList()
{
	return useList_;
}

DUBU::CachePacketManager::~CachePacketManager()
{
    Release();
}

void DUBU::CachePacketManager::Initialize()
{

}

void DUBU::CachePacketManager::Release()
{
    WriteLockGuard wl(lk_);
    while (!list_.empty())
    {
        CachePacketBuffer* ptr = list_.back();
        DUBU::Push<CachePacketBuffer>(ptr);
        list_.pop_back();
    }
}

DUBU::CachePacketBuffer* DUBU::CachePacketManager::PopPacketBuffer()
{
    WriteLockGuard wl(lk_);
    if (!list_.empty())
    {
        CachePacketBuffer* ptr = list_.back();
        list_.pop_back();
        useList_.insert(ptr);
        return ptr;
    }

    CachePacketBuffer* ptr = DUBU::Pop<CachePacketBuffer>();
    ptr->pos_ = ptr->buffer_;
    ptr->size_ = 0;
    return ptr;
}

void DUBU::CachePacketManager::PushPacketBuffer(CachePacketBuffer* ptr)
{
    WriteLockGuard wl(lk_);
    list_.push_back(ptr);
    useList_.erase(ptr);

    ZeroMemory(ptr->pos_, ptr->size_);
    ptr->size_ = 0;
}

void DUBU::CachePacketBuffer::Copy(const Uint8* ptr, const Uint16 len)
{
    std::memcpy(buffer_, ptr, len);
    pos_ = buffer_;
}
