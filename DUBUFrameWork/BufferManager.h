#pragma once
#include "pch.h"
#include "Singleton.h"
#include "OverlappedObj.h"

// 각 패킷의 크기는 고정이고, udp이므로 recvfrome완료 될때 내가 걸어둔 세션이랑 다르기 때문에 선언

namespace DUBU
{
	struct OverlappedPacketBuffer : public OverlappedObj
	{
		void* pos_ = nullptr;
		Int32 size_ = 0;
		Byte buffer_[PACKET_MAX_SIZE];
	};

	class PacketManager : public Singleton<PacketManager>
	{
	public:
		~PacketManager();
		void Initialize();

		OverlappedPacketBuffer* PopPacketBuffer();
		void PushPacketBuffer(OverlappedPacketBuffer* ptr);

		const Set<OverlappedPacketBuffer*>& GetUseList();

	private:
		const Int32 baseSize_ = MAX_CLIENT_COUNT;
		Vector<OverlappedPacketBuffer*> list_;
		Set<OverlappedPacketBuffer*> useList_;
	};
}
