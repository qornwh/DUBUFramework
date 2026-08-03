#include "EchoSessionHander.h"
#include "../extra/dubu_echo_packet_generated.h"
#include "RUDPSocket.h"
#include "EchoServer.h"
#include "Session.h"

EchoSessionHander::EchoSessionHander()
{
}

EchoSessionHander::~EchoSessionHander()
{
}

Bool EchoSessionHander::ChatVerifier(flatbuffers::Verifier& verifier)
{
    return DUBU::Echo::VerifyPacketBuffer(verifier);
}

void EchoSessionHander::ChatHandler(DUBU::Session* session, Uint8* buffer, Int32 size)
{
    DUBU::Packet::PacketHeader* header = reinterpret_cast<DUBU::Packet::PacketHeader*>(buffer);
    const Uint8 channelId = (header->flags_ & DUBU::Packet::PacketHeaderFlag::CHANNELMASK) >> 3;
    const DUBU::Echo::Packet* packet = DUBU::Echo::GetPacket(buffer + sizeof(DUBU::Packet::PacketHeader));

    if (packet->body_type() == DUBU::Echo::PacketBody_Chatting)
    {
        const DUBU::Echo::Chatting* chat = packet->body_as_Chatting();
        if (chat && chat->message())
        {
            const std::string& msg = chat->message()->str();
            spdlog::info("[CHAT] {}", msg);

            // BoradCast
            if (owner_ != nullptr)
            {
                // 브로드캐스트용 payload 1회 빌드
                flatbuffers::FlatBufferBuilder fbb;
                flatbuffers::Offset<DUBU::Echo::Chatting> chattingOffset = DUBU::Echo::CreateChattingDirect(fbb, chat->id(), msg.c_str());
                flatbuffers::Offset<DUBU::Echo::Packet> packetOffset = DUBU::Echo::CreatePacket(fbb, DUBU::Echo::PacketBody_Chatting, chattingOffset.Union());
                DUBU::Echo::FinishPacketBuffer(fbb, packetOffset);

                owner_->Broadcast(fbb.GetBufferPointer(), DUBU::Echo::PacketBody_Chatting, static_cast<Uint16>(fbb.GetSize()), session->GetSessionId(), channelId);
                // fbb 스코프 나갈시 메모리 헤제 RAII
            }
        }
    }
}

void EchoSessionHander::SetOwner(EchoServer* owner)
{
    owner_ = owner;
}


