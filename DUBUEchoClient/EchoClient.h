#pragma once
#include "Client.h"

/*
 * EchoClient : 채팅 테스트용 클라이언트
 */
class EchoClient : public DUBU::Client
{
public:
    EchoClient(const String& serverIP, Uint16 serverPort, const Map<Uint8, DUBU::Packet::PacketHandler>* handlers = nullptr);
    virtual ~EchoClient();

    // 접속 후 1회, 게이트웨이에 등록 요청
    void SendRegisterMessage();
    void SendChatMessage(const String& chat, Uint8 channelId = 1);
    void SendDumpChatMessage(const String& chat, Uint8 channelId = 0);

    // 부하 테스트용 봇 메시지 (송신 시각 링 기록 포함)
    void SendBotMessage(Uint32 count);
    // 봇 수신 집계 — 내 에코면 왕복 지연 계산 (담당 스레드에서만 호출)
    void RecvBotMessage(Uint32 id, Uint32 count);

    // 봇 집계 조회 (통계 출력 스레드에서 읽음)
    Uint64 GetBotRecvCount() const { return botRecvCount_.load(); }
    Uint64 GetBotRttSumMs() const { return botRttSumMs_.load(); }
    Uint64 GetBotRttSamples() const { return botRttSamples_.load(); }
    Uint32 GetBotRttMaxMs() const { return botRttMaxMs_.load(); }

    // 수신 핸들러가 현재 Dispatch 중인 클라를 찾기 위한 지정 (thread_local)
    static void SetCurrentBotClient(EchoClient* client);
    static EchoClient* GetCurrentBotClient();
private:
    // 봇 왕복 지연 측정용 송신 시각 링 (담당 스레드만 접근)
    static constexpr Uint32 BotRingSize = 64;
    Uint32 botSendNo_[BotRingSize] = {};
    Uint32 botSendTimeMs_[BotRingSize] = {};

    // 봇 수신/지연 집계
    Atomic<Uint64> botRecvCount_ = 0;
    Atomic<Uint64> botRttSumMs_ = 0;
    Atomic<Uint64> botRttSamples_ = 0;
    Atomic<Uint32> botRttMaxMs_ = 0;
};

