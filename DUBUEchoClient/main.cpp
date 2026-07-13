#include <iostream>
#include "EchoClient.h"
#include "EchoClientHander.h"
#include "Session.h"
#include "../extra/dubu_echo_packet_generated.h"

#include <windows.h>

static bool InterruptCtrlC = false;

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

int main(int argc, char** argv)
{
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
    EchoClientHander::GetInstance().SetOwner(client);
    
    // 5회 연결 요청
    client->ConnectTimes(5);

    // 연결확인
    if (!client->IsConnect())
    {
        spdlog::warn("Not Connect Server !!!");
        return 0;
    }

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