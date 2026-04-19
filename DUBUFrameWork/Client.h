#pragma once
#include "pch.h"
#include "RUDPSocket.h"

// 일단 최대 10번만 연결 시도
#define DEFAULT_REQUEST_CONNECT_NO 10

namespace DUBU
{
	class RUDPSocket;

	class Client : public ISocketHandler
	{
	public:
		Client(const String& serverIP, Uint16 serverPort);
		virtual ~Client();

		void Connect();
		void Disconnect();
		bool Dispatch();
		void OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size);
		void OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size);

		void ConnectMessage();
        void DisconnectMessage(Uint32 disconnectSeq);
		void SendEchoMessage();
		Uint32 GetClientId() const;

        // ping받고 pong으로 전달
        void RepeatPongMessage(Uint8* ptr, Uint16 size);

	private:
		Uint32 clientId_ = 0;
		Uint16 recvSequenceNo_ = 0;
		Uint16 sendSequenceNo_ = 0;

		std::shared_ptr<RUDPSocket> rudpSocket_;

        // 핑퐁 시퀀스넘버 체크 변수
        Uint32 lastPongSeq_ = 0;

        // 커넥션 카운트 체크 변수
        Uint32 connNo_ = 0;
	};
}

