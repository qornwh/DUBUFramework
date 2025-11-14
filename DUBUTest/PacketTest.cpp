#include "gtest/gtest.h"
#include "pch.h"
#include <iostream>
#include <thread>
#include <RWLock.h>
#include <functional>
#include "../extra/base_flatbuffer_generated.h"

#define PACKET_TEST(test_case_name, test_name) \
    TEST(test_case_name, test_name)

PACKET_TEST(DUBUTest, PacketTest) {
    using namespace DUBU;

	// Info 패킷 테스트
	{
		flatbuffers::FlatBufferBuilder fbb;

		auto code = Base::Sample::AnyPacket::AnyPacket_Info;
		auto header = Base::Sample::CreateHeader(fbb, 0, code);

		auto name = "test";
		std::vector<Base::Sample::Vec3> pos_arr;
		auto pos1 = Base::Sample::Vec3{ 5.5f, -1.1f, 0.f };
		auto pos2 = Base::Sample::Vec3{ 6.5f, -2.1f, 1.f };
		pos_arr.emplace_back(pos1);
		pos_arr.emplace_back(pos2);
		auto rot = Base::Sample::Vec3{ 12.1212f, -45.f, 180.f };
		auto info = Base::Sample::CreateInfoDirect(fbb, name, &pos_arr, &rot);
		auto content = info.Union();
		auto type = Base::Sample::AnyPacket::AnyPacket_Info;
		auto result = Base::Sample::CreatePacket(fbb, header, type, content);
		fbb.Finish(result);

		auto size = fbb.GetSize();
		EXPECT_FALSE(size > std::numeric_limits<int16_t>::max());

		auto fbb_ptr = fbb.GetBufferPointer();
		std::vector<uint8_t> cp_arr(size);
		std::copy(fbb_ptr, fbb_ptr + size, cp_arr.data());

		auto out_packet = Base::Sample::GetPacket(fbb_ptr);

		// VerifyXXX <= 이 함수가 읽을 수 있는지 확인이 되는 함수/ 제일 뒤에값이 첫번째 데이터인듯, 즉 size가 패킷길이 이상이면 성공 <= 이건 진짜 편하네
		flatbuffers::Verifier verifier(cp_arr.data(), size);
		EXPECT_TRUE(Base::Sample::VerifyPacketBuffer(verifier));

		auto out_type = out_packet->content_type();
		EXPECT_EQ(out_type, Base::Sample::AnyPacket_Info);
		auto out_header = out_packet->header();
		auto out_header_code = out_header->code();
		EXPECT_EQ(out_header_code, Base::Sample::AnyPacket_Info);
		auto out_info = out_packet->content_as_Info();
		auto out_name = out_info->name()->c_str();
		size_t len = sizeof(out_name);
		size_t out_len = sizeof(out_name);
		EXPECT_EQ(len, out_len);
		EXPECT_TRUE(std::equal(name, name + len, out_name));
	}

	// 멀티스레드 / 
}