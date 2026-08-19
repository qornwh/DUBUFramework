#pragma once
#include "pch.h"
#include "Config.h"
#include "Singleton.h"
#include "OverlappedObj.h"
#include "RWLock.h"

// 각 패킷의 크기는 고정이고, udp이므로 recvfrom완료 될 때, 내가 걸어둔 세션이랑 다르기 때문에 선언

namespace DUBU
{
	struct OverlappedPacketBuffer : public OverlappedObj
	{
		void* pos_ = nullptr;
		Uint16 size_ = 0;
		Uint8 buffer_[PACKET_MAX_SIZE];
	};

	class PacketManager : public Singleton<PacketManager>
	{
	public:
		~PacketManager();
		void Initialize();
		void Release();

		OverlappedPacketBuffer* PopPacketBuffer();
		void PushPacketBuffer(OverlappedPacketBuffer* ptr);

        // 모든 스레드 Join 후에만 호출할 것 (락 없이 내부 set 반환)
		const Set<OverlappedPacketBuffer*>& GetUseList();

	private:
		const Int32 baseSize_ = MAX_CLIENT_COUNT;
		Vector<OverlappedPacketBuffer*> list_;
		Set<OverlappedPacketBuffer*> useList_;
		Lock lk_;
	};

    // 캐시 패킷
    struct CachePacketBuffer
    {
        void* pos_ = nullptr;
        Uint16 size_ = 0;
        Uint8 buffer_[PACKET_MAX_SIZE];

        void Copy(const Uint8* ptr, const Uint16 size);
    };

    class CachePacketManager : public Singleton<CachePacketManager>
    {
    public:
        ~CachePacketManager();
        void Initialize();
        void Release();

        CachePacketBuffer* PopPacketBuffer();
        void PushPacketBuffer(CachePacketBuffer* ptr);

        const Set<CachePacketBuffer*>& GetUseList();

    private:
        Vector<CachePacketBuffer*> list_;
        Set<CachePacketBuffer*> useList_;
        Lock lk_;
    };
}
