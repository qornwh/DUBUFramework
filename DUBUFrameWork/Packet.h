#pragma once
#include "pch.h"
// Intel/AMD SSE 4.2 (CRC32 하드웨어 가속)
#include <nmmintrin.h>

namespace DUBU
{
    class Session;

	namespace Packet 
	{
		/*
		* NONE : 이동, 회전등 손실되면 감수하는 패킷
		* REPEAT : 스킬, 메시지등 손실시 재전송하는 패킷
		* CHUNK : 1개의 패킷이 나눠져서 들어오는 경우 (REPEAT 무조건)
		* CHANNEL : 채널 정보/순서보장(최대 2^3 까지만 가능 : 111 ~ 000, 디폴트 0채널) (REPEAT 무조건)
		* ACK : 재전송 확인용 패킷 (a -> b 패킷전송, b -> a 받았다는 확인)
		* SESSION : 연결 요청 세션이 생성됨 / sessionId 있을때는 연결 해제 세션 제거
		*/
		enum PacketHeaderFlag : Uint8
		{
			NONE        = 0b00000000,
			REPEAT      = 0b00000001,
			CHUNK       = 0b00000010,
            CHANNEL     = 0b00000100,
            CHANNELMASK = 0b00111000,
			ACK		    = 0b10000000,
			PING	    = 0b11000000,
			PONG        = 0b11100000,
			SESSION     = 0b11111110,
			DISCONNECT  = 0b11111111
		};

#pragma pack(push, 4)
        // 청크 정보
        struct ChunkInfo
        {
            Uint16 size_ = 0;
            Uint16 flag_ = 0; // ob10000000000000 & 연산시 포함되면 마지막이다. 최대 1.6k정도까지 사용된다.
        };

		struct PacketHeader
		{
			Uint16 totalSize_;
			Uint8 packetCode_;
            Uint8 flags_; // PacketHeaderFlag
			Uint32 sessionId_;
			Uint32 sequenceNo_;
			Uint32 timestamp_;
			Uint32 checksum_;
            ChunkInfo chunkInfo_;
		};

        // 보내는 패킷 옵션. 지정 초기화로 사용하기
        struct PacketOpctions
        {
            bool reliable_ = false;
            bool order_ = false;
            Uint8 channelID_ = 0;
            bool isChunck_ = false;
            Uint16 chunckFlag_ = 0;
            Uint16 chunckTotal_ = 0;
        };
#pragma pack(pop)

		class Packet
		{
		public:
			// 헤더 체크 / 복사
			static bool PacketHeaderCheck(const Uint8* ptr, const Uint16 len);
			static void PacketHeaderCopy(const Uint8* ptr, Uint8* dest);

			// 패킷 복사
			static void PacketCopy(const Uint8* ptr, const Uint16 len, Uint8* dest);

			// crc32 체크썸
			static Uint32 CRC32(const Uint8* ptr, Uint16 len);
		};

		struct PacketHandler
		{
			Function<bool(flatbuffers::Verifier&)> verifier_;
			Function<void(Session*, Uint8*, Int32)> handler_; // 클라는 세션이 없어서 포인터로...
            Function<void(Session*, Uint8*, Int32, Uint8*, Uint8)> handler2_; // 서브 핸들러용...(서브헤더 포인터, 타입 파라미터 2개 추가)
		};
	}
}

