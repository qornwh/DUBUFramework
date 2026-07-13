#pragma once
#include "pch.h"

namespace DUBU
{
    /*
     * 청크 패킷 정보
     */
    struct ChunkPacket
    {
        void Reset();
        bool SetBuffer(Uint16 flag, struct OverlappedPacketBuffer* buffer);

        struct OverlappedPacketBuffer* buffers_[DEFAULT_CHUNCK_MAX_SIZE];
        Uint16 flag_ = 0;
        Uint16 size_ = 0;
        bool isEnd = false;
    };
}
