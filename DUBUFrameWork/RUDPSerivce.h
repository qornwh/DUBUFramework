#pragma once
#include "pch.h"

// 이동만 허용해 준다.
class RUDPSerivce : public std::enable_shared_from_this<RUDPSerivce>
{
public:
	RUDPSerivce();
	RUDPSerivce(const RUDPSerivce& other) = delete;
	~RUDPSerivce();

	RUDPSerivce& operator=(const RUDPSerivce& other) = delete;

	void Start();
	void Update();

private:
	HANDLE iocpHd_;
	SOCKET serverSocket_ = INVALID_SOCKET;
	Int32 port_ = 13333;
};

