#pragma once
#include "pch.h"

class SessionManager
{
	// 여기서만 세션 생성, 등록, 삭제를 관리 / 다른곳에서 금지
public:
	SessionManager();
	SessionManager(const SessionManager& other) = delete;
	SessionManager(SessionManager&& other) = delete;
	~SessionManager();

	SessionManager& operator=(const SessionManager& other) = delete;
	SessionManager& operator=(SessionManager&& other) = delete;

private:
	Uint32 sessionCount_ = 0;
};

