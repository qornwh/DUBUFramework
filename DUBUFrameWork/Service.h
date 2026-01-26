#pragma once
#include "pch.h"

namespace DUBU 
{
	class RUDPServer;

	/*
	* 딱 한개의 서버만 들어감.
	*/
	class Service
	{
	public:
		Service();
		virtual ~Service();

		void Initialize();

	private:
		std::shared_ptr<RUDPServer> rudpServer_;
	};
}

