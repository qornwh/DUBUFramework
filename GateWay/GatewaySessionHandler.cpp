#include "GatewaySessionHandler.h"
#include "Session.h"
#include "../extra/dubu_echo_packet_generated.h"
#include "GatewayServer.h"
#include "Subheader.h"
#include "../extra/GatewaySubHeader.h"

GatewaySessionHandler::GatewaySessionHandler() : owner_(nullptr), clientToGatway_(false)
{
}

GatewaySessionHandler::~GatewaySessionHandler()
{
}

Bool GatewaySessionHandler::ChatVerifier(flatbuffers::Verifier& verifier)
{
    return DUBU::Echo::VerifyPacketBuffer(verifier);
}

void GatewaySessionHandler::ChatHandler(DUBU::Session* session, Uint8* buffer, Int32 size)
{
    const DUBU::Echo::Packet* packet = DUBU::Echo::GetPacket(buffer + sizeof(DUBU::Packet::PacketHeader));

    if (packet != nullptr && packet->body_type() == DUBU::Echo::PacketBody_Chatting)
    {
        const DUBU::Echo::Chatting* chat = packet->body_as_Chatting();
        if (chat && chat->message())
        {
            const std::string& msg = chat->message()->str();
            if (owner_ != nullptr)
            {
                if (session != nullptr)
                {
                    // 게이트 웨이서버 수신 성공 -> 에코서버 전송
                    spdlog::info("Gateway to Echo");
                    owner_->SendToEcho(buffer, DUBU::Echo::PacketBody_Chatting, size);
                }
                else
                {
                    // 에코 서버 -> 게이트 웨이
                    const Uint16 payloadSize = static_cast<Uint16>(size - sizeof(DUBU::Packet::PacketHeader));
                    spdlog::info("Echo to Gateway");
                    owner_->Broadcast(buffer + sizeof(DUBU::Packet::PacketHeader), DUBU::Echo::PacketBody_Chatting, payloadSize);
                }
            }
        }
    }
}

void GatewaySessionHandler::ChatHandler2(DUBU::Session* session, Uint8* buffer, Int32 size, Uint8* subBuf, Uint8 type)
{
    Uint64 offset = sizeof(DUBU::Packet::PacketHeader);
    GatewaySubHeader* sh = reinterpret_cast<GatewaySubHeader*>(buffer + offset);
    if (sh->type_ == type)
    {
        offset += sh->GetSize();
        Uint32 GatewaySessionID = sh->depthId1_;
        Uint32 serverSessionID = sh->depthId2_;

        const DUBU::Echo::Packet* packet = DUBU::Echo::GetPacket(buffer + offset);

        if (packet != nullptr && packet->body_type() == DUBU::Echo::PacketBody_Chatting)
        {
            const DUBU::Echo::Chatting* chat = packet->body_as_Chatting();
            if (chat && chat->message())
            {
                const std::string& msg = chat->message()->str();
                if (owner_ != nullptr)
                {
                    // 에코 서버 -> 게이트 웨이
                    const Uint16 payloadSize = static_cast<Uint16>(size - offset);
                    spdlog::info("Echo to Gateway {} - {}", serverSessionID, GatewaySessionID);
                    owner_->Broadcast(buffer + offset, DUBU::Echo::PacketBody_Chatting, payloadSize);
                }
            }
        }
    }
}

void GatewaySessionHandler::SetOwner(GatewayServer* owner)
{
    owner_ = owner;
}

void GatewaySessionHandler::SetClientToGatway(Bool isClientToGatway)
{
    clientToGatway_ = isClientToGatway;
}
