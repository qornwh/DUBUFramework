#pragma once
#include "pch.h"

namespace DUBU
{
	struct OverlappedPacketBuffer;

	enum OverlappedObjType : INT32
	{
		NONE = 0,
		RECVEFROM = 1,
		SENDTO = 2,
	};

	// GetQueuedCompletionStatut에서 OverlappedObj로 캐스팅해서 타입 알아낼려는 용도
	struct OverlappedObj
	{
		WSAOVERLAPPED overlapped_;
		OverlappedObjType type_ = OverlappedObjType::NONE;
		struct sockaddr_in remoteAddr_;
		int addrSize_ = sizeof(remoteAddr_);
		OverlappedPacketBuffer* ptr_ = nullptr;

		void Initialize();
		void SetType(OverlappedObjType type);
	};
}
