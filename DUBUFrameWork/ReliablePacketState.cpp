#include "ReliablePacketState.h"
#include "Packet.h"
#include "BufferManager.h"

void DUBU::ReliablePacketState::Reset()
{
    PacketStateBase::Reset();

    localWindowStart_ = 0;
    localSeqence_ = 0;

    // 미ACK pending 버퍼 반환까지 포함
    ReturnBuffers();

    for (Uint32 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
    {
        pendingPackets_[i] = { nullptr, 0, 0, false };
    }
}

bool DUBU::ReliablePacketState::AddPendingPacket(OverlappedPacketBuffer* opb, Uint16 size)
{
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);
    Uint32 timeStamp = header->timestamp_;
    Uint32 sequenceNo = header->sequenceNo_;
    bool isSent = false;

    Uint32 del = sequenceNo - localWindowStart_;
    if (del >= DEFAULT_WINDOW_COUNT)
    {
        // DEFAULT_WINDOW_COUNT 즉 캐싱가능한 크기 넘어가면 연결 끊음
        return false;
    }

    // ACK가져올때 까지 킵
    pendingPackets_[sequenceNo % DEFAULT_WINDOW_COUNT] = { opb, timeStamp, sequenceNo, isSent };

    // Pending 로컬 시퀀스 전진시킨다. 
    if (sequenceNo >= localSeqence_)
    {
        localSeqence_ = sequenceNo + 1;
    }
    return true;
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
        pendingPackets_[idx].buffer = nullptr;

        // RELIABLE을 끈다, "이 버퍼는 ACK 처리 끝남" 표시
        Uint32 old = pandingbuffer->type_.fetch_and(~OverlappedObjType::RELIABLE);
        if ((old & OverlappedObjType::SENDING) == 0)
        {
            // 커널이 손 뗀 상태이면 바로 반환
            PacketManager::GetInstance().PushPacketBuffer(pandingbuffer);
        }

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

void DUBU::ReliablePacketState::ReturnBuffers()
{
    for (Uint32 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
    {
        OverlappedPacketBuffer* buffer = pendingPackets_[i].buffer;
        if (buffer == nullptr)
        {
            continue;
        }
        pendingPackets_[i].buffer = nullptr;

        Uint32 old = buffer->type_.fetch_and(~OverlappedObjType::RELIABLE);
        if ((old & OverlappedObjType::SENDING) == 0)
        {
            PacketManager::GetInstance().PushPacketBuffer(buffer);
        }
    }
}
