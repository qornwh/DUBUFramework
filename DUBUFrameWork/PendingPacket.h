#pragma once
#include "pch.h"

namespace DUBU
{
	/*
	 * 재전송 패킷 정보
	 */
	struct PendingPacket
	{
		struct OverlappedPacketBuffer* buffer = nullptr;
		Uint32 timeStamp = 0;
		Uint32 sequenceNo = 0;
		bool isSent = false;
	};
};
