#include <thread>
#include <iostream>
#include "pch.h"
#include "Pool.h"
#include "BufferManager.h"
#include "EchoServer.h"
#include "EchoSessionHander.h"
#include "ThreadManager.h"
#include "Log.h"
#include "../extra/dubu_echo_packet_generated.h"

#include <windows.h>

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

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    DUBU::InitFileLog("server");
    DUBU::LoadConfig("./Config.json");
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
    handlers.emplace(
        DUBU::Echo::PacketBody_Register,
        DUBU::Packet::PacketHandler{
            // verifier_
            [](flatbuffers::Verifier& v) {
                return EchoSessionHander::GetInstance().RegisterVerifier(v);
            },
            // handler_
            [](DUBU::Session* session, Uint8* buf, Int32 len) {
                EchoSessionHander::GetInstance().RegisterHandler(session, buf, len);
            }
        }
    );
    handlers.emplace(
        DUBU::Echo::PacketBody_Bot,
        DUBU::Packet::PacketHandler{
            // verifier_
            [](flatbuffers::Verifier& v) {
                return EchoSessionHander::GetInstance().BotVerifier(v);
            },
            // handler_
            [](DUBU::Session* session, Uint8* buf, Int32 len) {
                EchoSessionHander::GetInstance().BotHandler(session, buf, len);
            }
        }
    );

    {
        EchoServer* server = new EchoServer();
        server->Initialize(&handlers);
        DUBU::ThreadManager::GetInstance().SetRecvLoop([&server, &handlers]() {
            EchoSessionHander::GetInstance().SetOwner(server);

            Uint32 time = DUBU::GetRelativeTimeMs();
            while (server->IsRunning())
            {
                Uint32 cur = DUBU::GetRelativeTimeMs();
                server->Dispatch();
            }
        });
        DUBU::ThreadManager::GetInstance().SetSessionLoop(nullptr);
        DUBU::ThreadManager::GetInstance().Start([]() {
            });

        while (true)
        {
            if (InterruptCtrlC)
            {
                server->Stop();
                break;
            }
        }
        DUBU::ThreadManager::GetInstance().Join();

        if (server != nullptr)
        {
            delete server;
            server = nullptr;
        }
    }
}