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

bool DUBU::ChunkPacket::SetBuffer(Uint16 flag, OverlappedPacketBuffer* buffer)
{
    if ((flag_ & flag) > 0)
    {
        return false;
    }

    flag_ |= flag;

    Uint32 count = 0;
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
