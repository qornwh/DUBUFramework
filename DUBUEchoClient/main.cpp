#include <iostream>
#include "EchoClient.h"
#include "EchoClientHander.h"
#include "Session.h"
#include "Log.h"
#include "../extra/dubu_echo_packet_generated.h"

#include <windows.h>
#include <fstream>

static volatile bool InterruptCtrlC = false;

BOOL WINAPI KeyBoarHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT)
    {
        // ctrl + c 입력 감지
        InterruptCtrlC = true;
        return true;
    }
    return FALSE;
}

// 스트레스 테스트(AI 생성)
void RunBotMode(const String& ip, Uint16 port, Uint32 intervalMs, Uint32 countPerThread, const Map<Uint8, DUBU::Packet::PacketHandler>* handlers)
{
    // 스레드는 4개
    constexpr Uint32 BotThreadCount = 4;
    spdlog::info("BOT mode : {}:{} {} thread x {} client, interval {}ms", ip, port, BotThreadCount, countPerThread, intervalMs);

    Atomic<Uint64> totalSent = 0;
    Atomic<Uint32> totalConnected = 0;
    // 통계 합산용 : 스레드별 클라 목록 (접속 완료 후 readyThreads로 읽기 허용)
    Vector<Vector<EchoClient*>> botClients(BotThreadCount);
    Atomic<Uint32> readyThreads = 0;

    Vector<std::thread> threads;
    threads.reserve(BotThreadCount);
    for (Uint32 t = 0; t < BotThreadCount; ++t)
    {
        threads.emplace_back([&, t]() {
            Vector<EchoClient*>& clients = botClients[t];
            clients.reserve(countPerThread);
            for (Uint32 i = 0; i < countPerThread; ++i)
            {
                EchoClient* client = new EchoClient(ip, port, handlers);
                client->ConnectTimes(5);
                if (!client->IsConnect())
                {
                    spdlog::error("BOT connect fail : thread {} idx {}", t, i);
                    delete client;
                    continue;
                }
                client->SendRegisterMessage();
                clients.push_back(client);
            }
            totalConnected.fetch_add(static_cast<Uint32>(clients.size()));
            readyThreads.fetch_add(1);
            spdlog::info("BOT thread {} : {}/{} connected", t, clients.size(), countPerThread);

            const size_t count = clients.size();
            if (count == 0)
            {
                return;
            }

            // 전송 시점 분산 (전원이 같은 틱에 몰리지 않게)
            Vector<Uint32> lastSend(count, 0);
            Vector<Uint32> sendNo(count, 0);
            Uint32 now = DUBU::GetRelativeTimeMs();
            for (size_t i = 0; i < count; ++i)
            {
                lastSend[i] = now - static_cast<Uint32>((Uint64)intervalMs * i / count);
            }

            while (true)
            {
                for (size_t i = 0; i < count; ++i)
                {
                    // 수신 핸들러가 내 에코를 매칭할 수 있게 현재 클라 지정
                    EchoClient::SetCurrentBotClient(clients[i]);

                    while (clients[i]->Dispatch(0)) {}

                    Uint32 cur = DUBU::GetRelativeTimeMs();
                    if (cur - lastSend[i] >= intervalMs)
                    {
                        clients[i]->SendBotMessage(++sendNo[i]);
                        lastSend[i] = cur;
                        totalSent.fetch_add(1);
                    }
                }
                std::this_thread::yield();
            }
        });
    }

    // 10초마다 요약 출력 (전 클라 집계 합산)
    while (true)
    {
        Sleep(10000);

        if (readyThreads.load() < BotThreadCount)
        {
            spdlog::info("[BOT-STATS] connecting... {}", totalConnected.load());
            continue;
        }

        Uint64 recv = 0;
        Uint64 rttSum = 0;
        Uint64 rttSamples = 0;
        Uint32 rttMax = 0;
        for (auto& clients : botClients)
        {
            for (EchoClient* client : clients)
            {
                recv += client->ExchangeBotRecvCount();
                rttSum += client->ExchangeBotRttSumMs();
                rttSamples += client->ExchangeBotRttSamples();
                Uint32 max = client->ExchangeBotRttMaxMs();
                if (max > rttMax)
                {
                    rttMax = max;
                }
            }
        }
        Uint64 rttAvg = (rttSamples > 0) ? (rttSum / rttSamples) : 0;

        spdlog::info("[BOT-STATS] connected={} sent={} recv={} rtt_avg={}ms rtt_max={}ms",
            totalConnected.load(), totalSent.exchange(0), recv, rttAvg, rttMax);
    }
}

int main(int argc, char** argv)
{
    DUBU::InitFileLog("gateway");
    SetConsoleOutputCP(CP_UTF8);

    // 메시지 핸들러
    Map<Uint8, DUBU::Packet::PacketHandler> handlers;
    handlers.emplace(
        DUBU::Echo::PacketBody_Chatting, 
        DUBU::Packet::PacketHandler{
            // verifier_
            [](flatbuffers::Verifier& v) {
                return EchoClientHander::GetInstance().ChatVerifier(v);
            },
            // handler_
            [](DUBU::Session* session, Uint8* buf, Int32 len) {
                EchoClientHander::GetInstance().ChatHandler(session, buf, len);
            }
        }
    );

    handlers.emplace(
        DUBU::Echo::PacketBody_Bot,
        DUBU::Packet::PacketHandler{
            // verifier_
            [](flatbuffers::Verifier& v) {
                return EchoClientHander::GetInstance().BotVerifier(v);
            },
            // handler_
            [](DUBU::Session* session, Uint8* buf, Int32 len) {
                EchoClientHander::GetInstance().BotHandler(session, buf, len);
            }
        }
    );

    // 스트레스 테스트 모드 : DUBUEchoClient.exe <ip> <port> <간격ms> <스레드당 클라수>  (총 클라수 = 클라수 x 4)
    if (argc >= 5)
    {
        RunBotMode(argv[1], static_cast<Uint16>(std::atoi(argv[2])), static_cast<Uint32>(std::atoi(argv[3])), static_cast<Uint32>(std::atoi(argv[4])), &handlers);
        return 0;
    }

    if (argc < 2 || argv == nullptr)
    {
        spdlog::error("Add Param Ip !!!");
        return 0;
    }
    else
    {
        spdlog::info("Connect Server Ip {}", argv[0]);
    }

    // 클라이언트
    EchoClient* client;
    client = new EchoClient(argv[1], 12346, &handlers);
    //client = new EchoClient("127.0.0.1", 12346, &handlers);
    EchoClientHander::GetInstance().SetOwner(client);
    
    // 5회 연결 요청
    client->ConnectTimes(5);

    // 연결확인
    if (!client->IsConnect())
    {
        spdlog::warn("Not Connect Server !!!");
        return 0;
    }

    // 에코 서버 등록 요청 (게이트웨이가 키 생성 후 전달)
    client->SendRegisterMessage();

    std::thread th([&client, &handlers]() {

        Uint32 time = DUBU::GetRelativeTimeMs();
        while (client->IsConnect())
        {
            client->Dispatch();
        }

        if (client != nullptr)
        {
            delete client;
            client = nullptr;
        }
    });

    // 1회 청크 테스트
    {
        std::ifstream file("../../../chunckTestText.txt");
        if (file.is_open())
        {
            Uint16 step = PACKET_MAX_SIZE - sizeof(DUBU::Packet::PacketHeader);
            String temp(step, 0);
            String test;
            int chunk_count = 1;

            Uint64 offset = 0;
            while (file.read(&temp[0], step) || file.gcount() > 0)
            {
                // 읽은 글자수
                Uint64 readSize = file.gcount();
                test.append(temp, 0, readSize);
                temp.assign(readSize, 0);

                offset += readSize;
            }
            file.close();

            if (client != nullptr && client->IsConnect())
            {
                // 청크 전송
                client->SendDumpChatMessage(test);
            }  
        }
        else
        {
            spdlog::warn("not found file.");
        }
    }
    
    String chat = "";
    chat.reserve(200);
    while (client != nullptr && client->IsConnect())
    {
        std::cout << "input >> ";
        if (!std::getline(std::cin, chat))
        {
            break;
        }

        if (!chat.empty())
        {
            client->SendChatMessage(chat, 2);
        }

        if (InterruptCtrlC)
        {
            if (client->IsConnect())
            {
                client->Disconnect();
            }
            break;
        }
    }
    th.join();

    return 0;
}