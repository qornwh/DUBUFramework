#pragma once
#include "pch.h"

namespace DUBU
{
    /*
     * 패킷 캐싱 정보
     */
    struct CachePacket
    {
        struct CachePacketBuffer* buffer = nullptr;
        Uint32 timeStamp = 0;
        Uint32 sequenceNo = 0;
        bool isKeep = false;
    };
};
