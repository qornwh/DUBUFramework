#include "ReliablePacketState.h"
#include "Packet.h"
#include "BufferManager.h"

void DUBU::ReliablePacketState::Reset()
{
    PacketStateBase::Reset();

    localWindowStart_ = 0;
    localSeqence_ = 0;

    for (Uint32 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
    {
        pendingPackets_[i] = { nullptr, 0, 0, false };
    }
}

void DUBU::ReliablePacketState::AddPendingPacket(OverlappedPacketBuffer* opb, Uint16 size)
{
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);
    Uint32 timeStamp = header->timestamp_;
    Uint32 sequenceNo = header->sequenceNo_;
    bool isSent = false;

    // ACK가져올때 까지 킵
    pendingPackets_[sequenceNo % DEFAULT_WINDOW_COUNT] = { opb, timeStamp, sequenceNo, isSent };

    // Pending 로컬 시퀀스 전진시킨다. 
    if (sequenceNo >= localSeqence_)
    {
        localSeqence_ = sequenceNo + 1;
    }
}

void DUBU::ReliablePacketState::AckProcess(Uint32 ackSeq, Uint32& rttMillisec_)
{
    Uint32 idx = ackSeq % DEFAULT_WINDOW_COUNT;

    if (pendingPackets_[idx].sequenceNo == ackSeq && pendingPackets_[idx].buffer != nullptr)
    {
        // RTT 갱신 : 비율 4 : 1
        Uint32 rtt = GetRelativeTimeMs() - pendingPackets_[idx].timeStamp;
        rttMillisec_ = (Uint32)(rttMillisec_ * 0.8f + rtt * 0.2f);

        // 수신 성공 버퍼 지운다.
        OverlappedPacketBuffer* pandingbuffer = pendingPackets_[idx].buffer;
        PacketManager::GetInstance().PushPacketBuffer(pandingbuffer);
        pendingPackets_[idx].buffer = nullptr;

        // pandding된 버퍼가 있는곳 까지 지운다, 단 localSeqence_까지만
        while (localWindowStart_ != localSeqence_ && pendingPackets_[localWindowStart_ % DEFAULT_WINDOW_COUNT].buffer == nullptr)
        {
            localWindowStart_++;
        }
    }
}

bool DUBU::ReliablePacketState::IsRepeat() const
{
    return localWindowStart_ != localSeqence_;
}
