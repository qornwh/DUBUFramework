#include "ChunkPacket.h"
#include "BufferManager.h"

void DUBU::ChunkPacket::Reset()
{
    for (Uint32 i = 0; i < DEFAULT_CHUNCK_MAX_SIZE; ++i)
    {
        buffers_[i] = 0;
        isEnd = false;
    }
}

bool DUBU::ChunkPacket::SetBuffer(Uint16 flag, CachePacketBuffer* buffer)
{
    if ((flag_ & flag) > 0)
    {
        return false;
    }

    flag_ |= flag;

    Uint8 count = 0;
    while (flag > 0)
    {
        if ((flag & 1) > 0)
        {
            buffers_[count] = buffer;
            break;
        }
        flag >>= 1;
    }

    if ((flag & DEFAULT_CHUNK_END_MASK) == DEFAULT_CHUNK_END_MASK)
    {
        isEnd = true;
    }
    return true;
}

bool DUBU::ChunkPacket::IsPull()
{
    // 0b1111'1111 + 1 == 0 이다.
    if (isEnd && 0 == (flag_ + 1))
    {
        return true;
    }
    return false;
}

DUBU::ChunkPacket& DUBU::ChunkPacketInj::Update(Uint64 seq, Uint16 flag)
{
    seq = StartSequnceNum(seq, flag);
    auto [it, inserted] = chunckPackets_.emplace(seq, ChunkPacket{});
    return it->second;
}

bool DUBU::ChunkPacketInj::Remove(Uint64 seq, Uint16 flag)
{
    seq = StartSequnceNum(seq, flag);
    auto it = chunckPackets_.find(seq);
    if (it != chunckPackets_.end())
    {
        chunckPackets_.erase(seq);
    }
    // 지우기 실패
    return false;
}

Uint64 DUBU::ChunkPacketInj::StartSequnceNum(Uint64 seq, Uint16 flag)
{
    Uint8 count = 0;
    while (flag > 0)
    {
        if ((flag & 1) > 0)
        {
            break;
        }
        flag >>= 1;
    }

    // 시작 0번째의 시퀀스 넘버가 나온다.
    return seq - count;
}
