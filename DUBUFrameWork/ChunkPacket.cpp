#include "ChunkPacket.h"
#include "BufferManager.h"

void DUBU::ChunkPacket::Reset()
{
    for (Uint32 i = 0; i < DEFAULT_CHUNCK_MAX_SIZE; ++i)
    {
        buffers_[i] = 0;
    }
    count_ = 0;
    isEnd = false;
}

bool DUBU::ChunkPacket::SetBuffer(Uint16 flag, CachePacketBuffer* buffer)
{
    if ((flag_ & flag) > 0)
    {
        return false;
    }

    // 사용개수 체크
    ++count_; 
    flag_ |= flag;

    if ((flag & DEFAULT_CHUNK_END_MASK) == DEFAULT_CHUNK_END_MASK)
    {
        isEnd = true;
    }

    Uint8 count = 0;
    while (flag > 0)
    {
        if ((flag & 1) > 0)
        {
            buffers_[count] = buffer;
            break;
        }
        flag >>= 1;
        ++count;
    }
    return true;
}

bool DUBU::ChunkPacket::IsPull()
{
    // 0b1111'1111 + 1 == 0 이다.
    if (isEnd && (0 == static_cast<Uint16>(flag_ + 1)))
    {
        return true;
    }
    return false;
}

DUBU::ChunkPacket& DUBU::ChunkPacketInj::Update(Uint32 seq, Uint16 flag)
{
    seq = StartSequnceNum(seq, flag);
    auto [it, inserted] = chunckPackets_.emplace(seq, ChunkPacket{});
    return it->second;
}

bool DUBU::ChunkPacketInj::Remove(Uint32 seq, Uint16 flag)
{
    seq = StartSequnceNum(seq, flag);
    return chunckPackets_.erase(seq) > 0;
}

Uint32 DUBU::ChunkPacketInj::StartSequnceNum(Uint32 seq, Uint16 flag)
{
    Uint8 count = 0;
    while (flag > 0)
    {
        if ((flag & 1) > 0)
        {
            break;
        }
        flag >>= 1;
        ++count;
    }

    // 시작 0번째의 시퀀스 넘버가 나온다.
    return seq - count;
}
