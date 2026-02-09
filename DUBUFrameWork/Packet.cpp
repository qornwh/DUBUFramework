#include "Packet.h"

bool DUBU::Packet::Packet::PacketHeaderCheck(const Uint8* ptr, const Uint16 len)
{
	if (sizeof(PacketHeader) <= len)
		return true;
	return false;
}

void DUBU::Packet::Packet::PacketHeaderCopy(const Uint16* ptr, std::vector<Uint8>& dest)
{
	constexpr Uint16 len = sizeof(PacketHeader);
	if (len > dest.size())
		dest.resize(len);
	std::copy(ptr, ptr + len, dest.data());
}

void DUBU::Packet::Packet::PacketCopy(const Uint8* ptr, const Uint16 len, std::vector<Uint8>& dest)
{
	if (len > dest.size())
		dest.resize(len);
	std::copy(ptr, ptr + len, dest.data());
}

Uint32 DUBU::Packet::Packet::CRC32(const Uint8* ptr, Uint16 len)
{
	// 하드웨어 가속을 이용한 CRC32 계산
	Uint32 crc = 0xFFFFFFFF; // 초기값
	Uint32 i = 0;

	// 4바이트씩 묶어서 빠르게 계산
	for (; i + 4 <= len; i += 4) {
		crc = _mm_crc32_u32(crc, *(Uint32*)(ptr + i));
	}

	// 남은 바이트 처리 (1~3바이트)
	for (; i < len; ++i) {
		crc = _mm_crc32_u8(crc, ptr[i]);
	}

	return crc ^ 0xFFFFFFFF; // 최종 XOR 처리
}
