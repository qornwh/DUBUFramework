#include "EchoClient.h"
#include "BufferManager.h"
#include "../extra/dubu_echo_packet_generated.h"

EchoClient::EchoClient(const String& serverIP, Uint16 serverPort, const Map<Uint8, DUBU::Packet::PacketHandler>* handlers) : DUBU::Client(serverIP, serverPort, handlers)
{
}

EchoClient::~EchoClient()
{
}

void EchoClient::SendRegisterMessage()
{
    // 키는 게이트웨이가 생성하므로 id는 클라 구분용 값만 넣는다
    flatbuffers::FlatBufferBuilder fbb;
    auto regist = DUBU::Echo::CreateRegister(fbb, GetClientId());
    auto packet = DUBU::Echo::CreatePacket(fbb, DUBU::Echo::PacketBody_Register, regist.Union());
    DUBU::Echo::FinishPacketBuffer(fbb, packet);

    Uint8* buffer = fbb.GetBufferPointer();
    Uint16 size = fbb.GetSize();

    SendPacket(buffer, DUBU::Echo::PacketBody_Register, size, DUBU::Packet::PacketOpctions{ true, true, 0 });
}

void EchoClient::SendChatMessage(const String& chat, Uint8 channelId)
{
    // 메시지 작성
    flatbuffers::FlatBufferBuilder fbb;
    auto chatting = DUBU::Echo::CreateChattingDirect(fbb, GetClientId(), chat.c_str());
    auto packet = DUBU::Echo::CreatePacket(fbb, DUBU::Echo::PacketBody_Chatting, chatting.Union());
    DUBU::Echo::FinishPacketBuffer(fbb, packet);
    // buffer, size
    Uint8* buffer = fbb.GetBufferPointer();
    Uint16 size = fbb.GetSize();

    SendPacket(buffer, DUBU::Echo::PacketBody_Chatting, size, DUBU::Packet::PacketOpctions{ true, true, channelId });
}

void EchoClient::SendDumpChatMessage(const String& chat, Uint8 channelId)
{
    // 메시지 작성
    flatbuffers::FlatBufferBuilder fbb;
    auto chatting = DUBU::Echo::CreateChattingDirect(fbb, GetClientId(), chat.c_str());
    auto packet = DUBU::Echo::CreatePacket(fbb, DUBU::Echo::PacketBody_Chatting, chatting.Union());
    DUBU::Echo::FinishPacketBuffer(fbb, packet);
    // buffer, size
    Uint8* buffer = fbb.GetBufferPointer();
    Uint16 size = fbb.GetSize();

    SendPacket(buffer, DUBU::Echo::PacketBody_Chatting, size, DUBU::Packet::PacketOpctions{ true, false, channelId });
}
