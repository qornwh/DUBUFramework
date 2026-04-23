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
	DUBU::PacketManager::GetInstance().Initialize(); 

	{
		DUBU::Server* server;
		std::thread th([&server]() {
			server = new DUBU::Server();
			server->Initialize();
			server->Run();
			delete server;
		});

        std::thread th2([]() {
            DUBU::Client client{ "127.0.0.1", SERVICE_PORT };
            client.ConnectTimes();

            Uint64 time = DUBU::GetCurrentTimeMs();
            Sleep(500);
            while (true)
            {
                Uint64 cur = DUBU::GetCurrentTimeMs();

                if (cur - time > 500)
                {
                    client.SendEchoMessage();
                    time = cur;
                }
                else
                {
                    client.Dispatch();
                }

                if (client.GetRecvSequenceNo() >= 10)
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