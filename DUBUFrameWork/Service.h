#pragma once
#include "pch.h"
#include "RWLock.h"

namespace DUBU 
{
	class RUDPSocket;

	/*
	* 딱 한개의 서버만 들어감.
	*/
	class Service
	{
	public:
		Service();
		virtual ~Service();

		void Initialize();
		void Run();

	private:
		std::shared_ptr<RUDPSocket> rudpServer_;
		Lock lk_;
		bool isRunning_ = false;
	};
}

