#include <thread>
#include <Server.h>
#include <Client.h>
#include <iostream>
#include "pch.h"
#include "Pool.h"
#include "BufferManager.h"

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
    DUBU::PacketManager::GetInstance().Initialize();

    {
        SetConsoleCtrlHandler(KeyBoarHandler, TRUE);
        DUBU::Server* server;
        std::thread th([&server]() {
            server = new DUBU::Server();
            server->Initialize();

            Uint64 time = DUBU::GetCurrentTimeMs();
            while (server->IsRunning())
            {
                Uint64 cur = DUBU::GetCurrentTimeMs();
                server->Dispatch();
            }
            if (server != nullptr)
                delete server;
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