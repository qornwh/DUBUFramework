#include <thread>
#include <Server.h>
#include <Client.h>
#include <iostream>
#include "Pool.h"
#include "BufferManager.h"

int main()
{
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
				client.ConnectMessage();
			}
		}

		server->Stop();
		th.join();
	}

	return 0;
}