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

void EchoClient::SendBotMessage(Uint32 count)
{
    // 에코 수신 시 왕복 지연 계산용 송신 시각 기록
    Uint32 idx = count % BotRingSize;
    botSendNo_[idx] = count;
    botSendTimeMs_[idx] = DUBU::GetRelativeTimeMs();

    flatbuffers::FlatBufferBuilder fbb;
    auto bot = DUBU::Echo::CreateBot(fbb, GetClientId(), count);
    auto packet = DUBU::Echo::CreatePacket(fbb, DUBU::Echo::PacketBody_Bot, bot.Union());
    DUBU::Echo::FinishPacketBuffer(fbb, packet);

    Uint8* buffer = fbb.GetBufferPointer();
    Uint16 size = fbb.GetSize();

    SendPacket(buffer, DUBU::Echo::PacketBody_Bot, size, DUBU::Packet::PacketOpctions{ true, false, 0 });
}

void EchoClient::RecvBotMessage(Uint32 id, Uint32 count)
{
    botRecvCount_.fetch_add(1);

    // 내가 보낸 메시지의 에코만 왕복 지연 집계
    if (id != GetClientId())
    {
        return;
    }

    Uint32 idx = count % BotRingSize;
    if (botSendNo_[idx] != count)
    {
        return;
    }

    Uint32 rtt = DUBU::GetRelativeTimeMs() - botSendTimeMs_[idx];
    botRttSumMs_.fetch_add(rtt);
    botRttSamples_.fetch_add(1);

    Uint32 prev = botRttMaxMs_.load();
    while (rtt > prev && !botRttMaxMs_.compare_exchange_weak(prev, rtt));
}

// 현재 Dispatch 중인 클라 (스레드마다 자기 담당 클라)
static thread_local EchoClient* t_currentBotClient = nullptr;

void EchoClient::SetCurrentBotClient(EchoClient* client)
{
    t_currentBotClient = client;
}

EchoClient* EchoClient::GetCurrentBotClient()
{
    return t_currentBotClient;
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
