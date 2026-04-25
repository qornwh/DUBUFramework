#include <iostream>
#include "EchoClient.h"

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

    EchoClient client{ "127.0.0.1", SERVICE_PORT };
    client.ConnectTimes();

    Uint64 time = DUBU::GetCurrentTimeMs();
    Sleep(500);
    
    String chat = "";
    chat.reserve(200);
    while (true)
    {
        std::cout << "input >> ";
        if (!std::getline(std::cin, chat))
        {
            break;
        }

        if (chat.empty())
        {
            continue;
        }

        client.SendChatMessage(chat);
        if (InterruptCtrlC)
        {
            if (client.IsConnect())
            {
                client.Disconnect();
            }
            break;
        }
    }
    return 0;
}