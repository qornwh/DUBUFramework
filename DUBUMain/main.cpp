#include <thread>
#include <Server.h>
#include <Client.h>
#include <iostream>
#include "pch.h"
#include "Pool.h"
#include "BufferManager.h"

#include <windows.h>

int main()
{
    SetConsoleOutputCP(CP_UTF8);
   
    DUBU::Initialize();

	{
		DUBU::Server* server;
		std::thread th([&server]() {
			server = new DUBU::Server();
			server->Initialize(nullptr);

            Uint32 time = DUBU::GetRelativeTimeMs();
            while (true)
            {
                Uint32 cur = DUBU::GetRelativeTimeMs();
                server->Dispatch();
                if (cur - time >= 10000)
                    break;
            }
            if (server != nullptr)
			    delete server;
		});

        std::thread th2([]() {
            DUBU::Client client{ "127.0.0.1", DUBU::g_servicePort };
            client.ConnectTimes();

            const Uint32 TimeOut = 20000;
            Uint32 time = DUBU::GetRelativeTimeMs();
            Uint32 firstTime = time;
            Sleep(500);
            while (true)
            {
                Uint32 cur = DUBU::GetRelativeTimeMs();

                if (cur - time > 500)
                {
                    client.SendEchoMessage();
                    time = cur;
                }
                else
                {
                    client.Dispatch();
                }

                if (cur - firstTime > TimeOut)
                {
                    client.Disconnect();
                    break;
                }
            }
        });

		th.join();
        th2.join();
        server->Stop();
	}

	return 0;
}