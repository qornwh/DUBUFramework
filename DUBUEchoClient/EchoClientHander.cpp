#include "EchoClientHander.h"
#include "../extra/dubu_echo_packet_generated.h"
#include "RUDPSocket.h"
#include "Packet.h"
#include "Session.h"

EchoClientHander::EchoClientHander()
{
}

EchoClientHander::~EchoClientHander()
{
}

Bool EchoClientHander::ChatVerifier(flatbuffers::Verifier& verifier)
{
    return DUBU::Echo::VerifyPacketBuffer(verifier);
}

void EchoClientHander::ChatHandler(DUBU::Session* session, Uint8* buffer, Int32 size)
{
    const DUBU::Echo::Packet* packet = DUBU::Echo::GetPacket(buffer + sizeof(DUBU::Packet::PacketHeader));

    if (packet->body_type() == DUBU::Echo::PacketBody_Chatting)
    {
        auto chat = packet->body_as_Chatting();
        if (chat && chat->message())
        {
            const std::string& msg = chat->message()->str();
            spdlog::info("[CHAT-Recive] {} ", msg);
        }
    }
}

void EchoClientHander::SetOwner(EchoClient* owner)
{
    owner_ = owner;
}
