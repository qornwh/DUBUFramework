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

  //  struct RepeatPacketState
  //  {
  //      Uint32 currentRepeatSeq = 0;
  //      Uint32 lastRepeatSeq = 0;
  //      Uint64 cacheRepeatCount = 0;
  //      Uint32 sendRepeatSeq = 0;

		//Uint32 UpdateSendSequenceNo();
  //      Uint32 GetRecvSequenceNo() const;
  //      Uint32 GetSendSequenceNo() const;
  //  };
};
