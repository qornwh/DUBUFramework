#include "PacketStateBase.h"

void DUBU::PacketStateBase::Reset()
{
    recvRepeatSeq_ = 0;
    lastRepeatSeq_ = 0;
    cacheRepeatCount_ = 0;
    sendRepeatSeq_ = 0;
#ifdef TEST_MODE
    recvCount_ = 0;
#endif
}

void DUBU::PacketStateBase::UpdateRecvSequenceNo(Uint32 sequenceNo)
{
    recvRepeatSeq_ = sequenceNo;
#ifdef TEST_MODE
    ++recvCount_;
#endif
}

Uint32 DUBU::PacketStateBase::UpdateSendSequenceNo()
{
    return ++sendRepeatSeq_;
}

Uint32 DUBU::PacketStateBase::GetRecvSequenceNo() const
{
    return recvRepeatSeq_;
}

Uint32 DUBU::PacketStateBase::GetSendSequenceNo() const
{
    return sendRepeatSeq_;
}
