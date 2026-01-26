#pragma once
#include "pch.h"

/*
* 실제 유저들의 정보를 관리하는 세션 객체
* 이거는 스마트 포인터를 설정하지 않는다.
*/

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
		Int32 sessionId_ = -1;
	};

}
