#pragma once
#include "pch.h"
// Intel/AMD SSE 4.2 (CRC32 하드웨어 가속)
#include <nmmintrin.h>

namespace DUBU
{
	namespace Packet 
	{
		/*
		* NONE : 이동, 회전등 손실되면 감수하는 패킷
		* REPEAT : 스킬, 메시지등 손실시 재전송하는 패킷
		* CHUNK : 1개의 패킷이 나눠져서 들어오는 경우 (초기 로드시 전체 정보, 최대 2^5 까지만 가능
		* LASTCHUNK : CHUNK로 나눠진 패킷의 마지막 패킷
		* ACK : 재전송 확인용 패킷 (a -> b 패킷전송, b -> a 받았다는 확인)
		* SESSION : 연결 요청 세션이 생성됨 / sessionId 있을때는 연결 해제 세션 제거
		*/
		enum PacketHeaderFlag : Uint8
		{
			NONE      = 0b00000000,
			REPEAT    = 0b00000001,
			CHUNK     = 0b00000010,
			LASTCHUNK = 0b10000000,
			ACK		  = 0b11000000,
			PING	  = 0b11100000,
			PONG      = 0b11110000,
			SESSION   = 0b11111111,
		};

		struct PacketHeader
		{
			Uint16 totalSize_;
			Uint8 packetCode_;
			PacketHeaderFlag flags_;
			Uint32 sessionId_;
			Uint32 sequenceNo_;
			Uint32 timestamp_;
			Uint32 checksum_;
		};

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
			Function<void(Uint8*, Int32)> handler_;
		};
	}
}

