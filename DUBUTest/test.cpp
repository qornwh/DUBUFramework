#include "pch.h"
#include <iostream>
#include <thread>
#include <RWLock.h>
#include <Pool.h>
#include <BufferManager.h>
#include <Service.h>


TEST(DUBUTest, Sample) 
{
	EXPECT_EQ(1, 1);
	EXPECT_TRUE(true);
}

TEST(DUBUTest, SocketTest)
{
	DUBU::Initialize();
	DUBU::PacketManager::GetInstance().Initialize();

	DUBU::Server service;
	service.Initialize();


}