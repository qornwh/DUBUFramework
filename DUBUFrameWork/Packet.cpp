#include "Packet.h"

void DUBU::Packet::PacketCopy(const uint8_t* ptr, const uint16_t len, std::vector<uint8_t>& copy_array)
{
	if (len > copy_array.size())
		copy_array.resize(len);
	std::copy(ptr, ptr + len, copy_array.data());
}
