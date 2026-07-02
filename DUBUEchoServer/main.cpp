#include <thread>
#include <iostream>
#include "pch.h"
#include "Pool.h"
#include "BufferManager.h"
#include "EchoServer.h"
#include "EchoSessionHander.h"
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

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    DUBU::Initialize();

    // 메시지 핸들러
    Map<Uint8, DUBU::Packet::PacketHandler> handlers;
    handlers.emplace(
        DUBU::Echo::PacketBody_Chatting,
        DUBU::Packet::PacketHandler{
            // verifier_
            [](flatbuffers::Verifier& v) {
                return EchoSessionHander::GetInstance().ChatVerifier(v);
            },
            // handler_
            [](DUBU::Session* session, Uint8* buf, Int32 len) {
                EchoSessionHander::GetInstance().ChatHandler(session, buf, len);
            }
        }
    );

    {
        EchoServer* server = new EchoServer();
        std::thread th([&server, &handlers]() {
            server->Initialize(&handlers);
            EchoSessionHander::GetInstance().SetOwner(server);

            Uint32 time = DUBU::GetRelativeTimeMs();
            while (server->IsRunning())
            {
                Uint32 cur = DUBU::GetRelativeTimeMs();
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