#pragma once
#include "PacketStateBase.h"
#include "PendingPacket.h"

namespace DUBU
{
    struct ReliablePacketState : public PacketStateBase
    {
        PendingPacket pendingPackets_[DEFAULT_WINDOW_COUNT];
        Uint32 localWindowStart_ = 0;
        Uint32 localSeqence_ = 0;

        void Reset() override;
        void AddPendingPacket(struct OverlappedPacketBuffer* opb, Uint16 size);
        void AckProcess(Uint32 ackSeq, Uint32 rttMillisec_);
        bool IsRepeat() const;
    };
}
