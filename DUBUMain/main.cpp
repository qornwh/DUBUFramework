#include <windows.h>

#include <thread>
#include <Server.h>
#include <Client.h>
#include <iostream>
#include "pch.h"
#include "Pool.h"
#include "BufferManager.h"

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

		DUBU::Client client{ "127.0.0.1", SERVICE_PORT };
		client.Connect();

        Uint64 time = 0;

        Uint64 cur = DUBU::GetCurrentTimeMs();

        while (true)
        {
            bool ret = client.Dispatch();
            if (ret && client.GetClientId() > 0)
            {
                spdlog::info("ClientID : {}", client.GetClientId());
                break;
            }
            else
            {
                Sleep(500);
                client.ConnectMessage();
            }
        }

        while (client.GetClientId())
        {
            bool ret = client.Dispatch();

            if (DUBU::GetCurrentTimeMs() - cur > 60000)
            {
                break;
            }

        }

		server->Stop();
		th.join();
	}

	return 0;
}