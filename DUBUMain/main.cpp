#include <iostream>
#include "Pool.h"
#include "Singleton.h"
#include "RWLock.h"
#include <thread>
#include <functional>

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

int main()
{
	DUBU::Init();
	AAA& aaa = *DUBU::Pop<AAA*>();
	// placement new
	BBB* bbb = new (DUBU::Pop<BBB*>()) BBB();
	CCC* ccc = DUBU::Pop<CCC*>();
	DDD* ddd = DUBU::Pop<DDD*>();

	aaa.a = 12;
	ddd->a = 12;

	DUBU::Push<AAA*>(&aaa);
	DUBU::Push<BBB*>(bbb);
	DUBU::Push<CCC*>(ccc);
	DUBU::Push<DDD*>(ddd);

	DUBU::Singleton<AAA> obj_AAA;
	auto& ref_AAA = obj_AAA.GetInstance();
	ref_AAA.a = 26;
	printf("%d\n", obj_AAA.GetInstance().a);

	DUBU::Lock l{};

	return 0;
}