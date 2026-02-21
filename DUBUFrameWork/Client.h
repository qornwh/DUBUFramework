#pragma once
#include "pch.h"
#include "RUDPSocket.h"

namespace DUBU
{
	class RUDPSocket;

	class Client : public ISocketHandler
	{
	public:
		Client(const String& serverIP, Uint16 serverPort);
		virtual ~Client();

		bool Dispatch();
		void SendTo(Uint8* ptr, Int32 size);
		void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size);
		void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size);

	private:
		Uint32 clientId_ = 0;
		Uint32 recvSequenceNo_ = 0;
		Uint32 sendSequenceNo_ = 0;

		std::shared_ptr<RUDPSocket> rudpSocket_;
	};
}

