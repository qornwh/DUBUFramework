#include "EchoClient.h"
#include "BufferManager.h"
#include "../extra/dubu_echo_packet_generated.h"

EchoClient::EchoClient(const String& serverIP, Uint16 serverPort, const Map<Uint8, DUBU::Packet::PacketHandler>* handlers) : DUBU::Client(serverIP, serverPort, handlers)
{
}

EchoClient::~EchoClient()
{
}

void EchoClient::SendChatMessage(const String& chat)
{
    // 메시지 작성
    flatbuffers::FlatBufferBuilder fbb;
    auto chatting = DUBU::Echo::CreateChattingDirect(fbb, chat.c_str());
    auto packet = DUBU::Echo::CreatePacket(fbb, DUBU::Echo::PacketBody_Chatting, chatting.Union());
    DUBU::Echo::FinishPacketBuffer(fbb, packet);
    // buffer, size
    Uint8* buffer = fbb.GetBufferPointer();
    size_t size = fbb.GetSize();

    DUBU::OverlappedPacketBuffer* opb = DUBU::PacketManager::GetInstance().PopPacketBuffer();
    DUBU::Packet::PacketHeader* header = reinterpret_cast<DUBU::Packet::PacketHeader*>(opb->buffer_);

    // 헤더 작성
    header->checksum_ = 0;
    header->flags_ = DUBU::Packet::PacketHeaderFlag::REPEAT;
    header->totalSize_ = static_cast<Uint16>(sizeof(DUBU::Packet::PacketHeader) + size);
    header->sessionId_ = GetClientId();
    header->sequenceNo_ = UpdateSendSequenceNo();
    header->timestamp_ = DUBU::GetCurrentTimeMs();
    header->packetCode_ = 0;

    // 메시지 복사
    std::memcpy(opb->buffer_ + sizeof(DUBU::Packet::PacketHeader), buffer, size);

    // 전체 패킷 사이즈 설정
    opb->size_ = header->totalSize_;

    // crc32 암호화
    Uint32 checksum = DUBU::Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
    header->checksum_ = checksum;
    rudpSocket_->SendToReliable(rudpSocket_->GetSockAddr(), opb);
    AddPendingPacket(opb, opb->size_);
}
