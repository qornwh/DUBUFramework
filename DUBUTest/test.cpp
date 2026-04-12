#include "pch.h"
#include <iostream>
#include <thread>
#include <RWLock.h>
#include <Pool.h>
#include <BufferManager.h>
#include <Server.h>

class AAA
{
public:
	int a;
};
class BBB
{
public:
	BBB() {
		std::cout << "BBB" << '\n';
	}
	int a;
	short b;
};
class CCC
{
public:
	int a;
	short b;
	char c;
};
class DDD : public CCC
{
public:
	long long d;
};

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

	AAA& aaa = *DUBU::Pop<AAA>();
	// placement new
	BBB* bbb = new (DUBU::Pop<BBB>()) BBB();
	CCC* ccc = DUBU::Pop<CCC>();
	DDD* ddd = DUBU::Pop<DDD>();

	aaa.a = 12;
	ddd->a = 12;

	DUBU::Push<AAA>(&aaa);
	DUBU::Push<BBB>(bbb);
	DUBU::Push<CCC>(ccc);
	DUBU::Push<DDD>(ddd);

	DUBU::Singleton<AAA> obj_AAA;
	auto& ref_AAA = obj_AAA.GetInstance();
	ref_AAA.a = 26;
	printf("%d\n", obj_AAA.GetInstance().a);

	DUBU::Lock l{};
}