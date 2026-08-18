#pragma once
#include "PacketStateBase.h"
#include "PendingPacket.h"
#include "CachePacket.h"

namespace DUBU
{
    struct ReliablePacketState : public PacketStateBase
    {
        PendingPacket pendingPackets_[DEFAULT_WINDOW_COUNT];
        Uint32 localWindowStart_ = 0;
        Uint32 localSeqence_ = 0;

        void Reset() override;
        void AddPendingPacket(struct OverlappedPacketBuffer* opb, Uint16 size);
        void AckProcess(Uint32 ackSeq, Uint32& rttMillisec_);
        bool IsRepeat() const;
        // 종료시 반환용
        void ReturnBuffers();
    };

    struct CacheAlreadyPacket
    {
        // 수신쪽
        ReliablePacketState reliablePacketState;
        // 수신후 순서대로 키핑
        CachePacket cachePackets[DEFAULT_WINDOW_COUNT];
    };
}
