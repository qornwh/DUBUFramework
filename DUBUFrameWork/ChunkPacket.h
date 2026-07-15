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
        bool SetBuffer(Uint16 flag, struct CachePacketBuffer* buffer);
        bool IsPull();

        struct CachePacketBuffer* buffers_[DEFAULT_CHUNCK_MAX_SIZE];
        Uint16 flag_ = 0;
        Uint16 size_ = 0;
        Uint16 count_ = 0;
        bool isEnd = false;
    };

    /*
    * 청크 패킷 인잭션
    *  - 세션, 클라에서 사용하는 청크패킷 임시 캐싱
    */
    class ChunkPacketInj
    {
    public:
        ChunkPacket& Update(Uint64 seq, Uint16 flag = 0);
        bool Remove(Uint64 seq, Uint16 flag = 0);

    private:
        Uint64 StartSequnceNum(Uint64 seq, Uint16 flag);
        Map<Uint64, ChunkPacket> chunckPackets_;
    };
}
