#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flatbuffer_builder.h"
#include "base_flatbuffer_generated.h"
#include <iostream>
#include <fstream>
#include <xmemory>
#include <vector>

void func1(flatbuffers::FlatBufferBuilder&& fbb)
{
	auto size = fbb.GetSize();
	std::cout << "  --  " << size << '\n';
}

int main11() 
{
	std::function<void()> _func;
	{
		flatbuffers::FlatBufferBuilder fbb;

		auto code = Base::Sample::AnyPacket::AnyPacket_Info;
		auto header = Base::Sample::CreateHeader(fbb, 0, code);

		auto name = "test";
		std::vector<Base::Sample::Vec3> pos_arr;
		auto pos1 = Base::Sample::Vec3{ 5.5, -1.1, 0 };
		auto pos2 = Base::Sample::Vec3{ 6.5, -2.1, 1 };
		pos_arr.emplace_back(pos1);
		pos_arr.emplace_back(pos2);
		auto rot = Base::Sample::Vec3{ 12.1212, -45, 180 };
		auto info = Base::Sample::CreateInfoDirect(fbb, name, &pos_arr, &rot);
		auto content = info.Union();
		auto type = Base::Sample::AnyPacket::AnyPacket_Info;

		auto result = Base::Sample::CreatePacket(fbb, header, type, content);

		fbb.Finish(result);
		auto size = fbb.GetSize();
		std::cout << "  --  " << size << '\n';

		// 여기서 header의 size에 값을 다시 할당은?
		auto fbb_ptr = fbb.GetBufferPointer();

		//if (size > std::numeric_limits<int16_t>::max()) {
		//	std::cerr << "경고: 최종 크기(" << size << ")가 short 범위를 초과합니다. 데이터 손실 발생.\n";
		//}

		std::vector<uint8_t> cp_arr(size);
		std::copy(fbb_ptr, fbb_ptr + size, cp_arr.data());

		// VerifyXXX <= 이 함수가 읽을 수 있는지 확인이 되는 함수/ 제일 뒤에값이 첫번째 데이터인듯, 즉 size가 패킷길이 이상이면 성공 <= 이건 진짜 편하네
		auto out_packet = Base::Sample::GetPacket(fbb_ptr);

		flatbuffers::Verifier verifier(cp_arr.data(), size);
		if (Base::Sample::VerifyPacketBuffer(verifier))
		{
			std::cout << "ok" << '\n';
			//return 0;
		}

		auto&& aaaa = std::move(fbb);

		auto out_header = out_packet->header();
		auto out_type = out_packet->content_type();
		switch (out_type)
		{
		case Base::Sample::AnyPacket_Info:
			std::cout << "inof \n";
			// 그냥 값 복사해야된다
			// 타입에 맞는 최상위 클래스 만들고 그거 캐스팅 해야 될듯하다
			break;
		case Base::Sample::AnyPacket_Message:
			std::cout << "mess \n";
			break;
		}
	}

	_func();
	// 멀티스레드 / 

	return 0;
}