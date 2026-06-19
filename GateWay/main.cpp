#include <thread>
#include <iostream>
#include "pch.h"
#include "Pool.h"
#include "BufferManager.h"
#include "GatewayServer.h"
#include "GatewaySessionHandler.h"
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

    DUBU::LoadConfig("./Config.json");
    DUBU::Initialize();
    DUBU::PacketManager::GetInstance().Initialize();

    // 메시지 핸들러
    Map<Uint8, DUBU::Packet::PacketHandler> handlers;
    handlers.emplace(
        DUBU::Echo::PacketBody_Chatting,
        DUBU::Packet::PacketHandler{
            // verifier_
            [](flatbuffers::Verifier& v) {
                return GatewaySessionHandler::GetInstance().ChatVerifier(v);
            },
        // handler_
        [](Uint8* buf, Int32 len) {
            GatewaySessionHandler::GetInstance().ChatHandler(buf, len);
        }
        }
    );

    {
        GatewayServer* server = new GatewayServer();
        std::thread th([&server, &handlers]() {
            server->Initialize(&handlers);
            GatewaySessionHandler::GetInstance().SetOwner(server);

            Uint64 time = DUBU::GetCurrentTimeMs();
            while (server->IsRunning())
            {
                Uint64 cur = DUBU::GetCurrentTimeMs();
                server->Dispatch();
            }
            if (server != nullptr)
            {
                delete server;
                server = nullptr;
            }
        });

        while (true)
        {
            if (InterruptCtrlC)
            {
                server->Stop();
                break;
            }
        }
        th.join();
    }
}