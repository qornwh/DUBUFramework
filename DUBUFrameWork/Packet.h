#pragma once
#include "pch.h"
#include <vector>

namespace DUBU
{
	class Packet
	{
	public:
		template<typename T>
		static bool PacketCheck(flatbuffers::Verifier& verifier);
		static void PacketCopy(const uint8_t* ptr, const uint16_t len, std::vector<uint8_t>& copy_array);
	};

	template<typename T>
	inline bool Packet::PacketCheck(flatbuffers::Verifier& verifier)
	{
		if (T::VerifyPacketBuffer(verifier))
			return true;
		return false;
	}
}

