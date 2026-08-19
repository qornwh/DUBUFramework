#include <thread>
#include <iostream>
#include "pch.h"
#include "Pool.h"
#include "BufferManager.h"
#include "GatewayServer.h"
#include "GatewaySessionHandler.h"
#include "Session.h"
#include "ThreadManager.h"
#include "../extra/dubu_echo_packet_generated.h"

#include <windows.h>

// 게이트웨이 테스트용 코드

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
            [](DUBU::Session* session, Uint8* buf, Int32 len) {
                GatewaySessionHandler::GetInstance().ChatHandler(session, buf, len);
            },
            // handler2_
            [](DUBU::Session* session, Uint8* buf, Int32 len, Uint8* subBuf, Uint8 type) {
                GatewaySessionHandler::GetInstance().ChatHandler2(session, buf, len, subBuf, type);
            }
        }
    );
    handlers.emplace(
        DUBU::Echo::PacketBody_Register,
        DUBU::Packet::PacketHandler{
            // verifier_
            [](flatbuffers::Verifier& v) {
                return GatewaySessionHandler::GetInstance().RegisterVerifier(v);
            },
            // handler_
            [](DUBU::Session* session, Uint8* buf, Int32 len) {
                GatewaySessionHandler::GetInstance().RegisterHandler(session, buf, len);
            },
            // handler2_
            [](DUBU::Session* session, Uint8* buf, Int32 len, Uint8* subBuf, Uint8 type) {
                GatewaySessionHandler::GetInstance().RegisterHandler2(session, buf, len, subBuf, type);
            }
        }
    );

    {
        GatewayServer* server = new GatewayServer();
        server->Initialize(&handlers);
        DUBU::ThreadManager::GetInstance().SetRecvLoop([&server, &handlers]() {
            GatewaySessionHandler::GetInstance().SetOwner(server);

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
        DUBU::ThreadManager::GetInstance().SetSessionLoop(nullptr);
        DUBU::ThreadManager::GetInstance().Start([&server]() {
            while (server && server->IsRunning())
            {
                Uint32 cur = DUBU::GetRelativeTimeMs();
                server->InnerDispatch();
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
        DUBU::ThreadManager::GetInstance().Join();
    }
}