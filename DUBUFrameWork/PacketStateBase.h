#pragma once
#include "pch.h"

namespace DUBU
{
    struct PacketStateBase
    {
        Uint32 recvRepeatSeq_ = 0;      // 현재 수신된 가장 낮은 시퀀스 넘버
        Uint32 lastRepeatSeq_ = 0;      // 현재 수신된 가장 높은 시퀀스 넘버
        Uint64 cacheRepeatCount_ = 0;   // 현재 수신된 시퀀스 넘버 64비트 관리
        Uint32 sendRepeatSeq_ = 0;      // 현재 송신된 시퀀스 넘버

        virtual void Reset();
        Uint32 UpdateSendSequenceNo();
        Uint32 GetRecvSequenceNo() const;
        Uint32 GetSendSequenceNo() const;
    };
}
