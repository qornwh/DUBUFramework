#pragma once
#include "pch.h"

namespace DUBU
{
	class Session
	{
	public:
		Session();
		virtual ~Session();

		void SetId(Int32 id);
		virtual void Reset();

	private:
		Uint32 sessionId_ = 0;
		Uint32 recvSequenceNo_ = 0;
		Uint32 sendSequenceNo_ = 0;
	};
}
