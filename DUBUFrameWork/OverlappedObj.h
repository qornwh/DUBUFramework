#pragma once
#include "pch.h"

namespace DUBU
{
	struct OverlappedPacketBuffer;

	enum OverlappedObjType : Uint32
	{
		NONE = 0x0000'0000,
		RECVEFROM = 0x0000'0001,
		SENDTO = 0x0000'0002,
		RELIABLE = 0xFFFF'0000
	};

	// GetQueuedCompletionStatut에서 OverlappedObj로 캐스팅해서 타입 알아낼려는 용도
	struct OverlappedObj
	{
		WSAOVERLAPPED overlapped_;
		Uint32 type_ = OverlappedObjType::NONE;
		struct sockaddr_in remoteAddr_;
		Int32 addrSize_ = sizeof(remoteAddr_);
		OverlappedPacketBuffer* ptr_ = nullptr;

		void Initialize();
		void SetType(Uint32 type);
	};
}
