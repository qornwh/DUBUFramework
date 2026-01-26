#include "Service.h"
#include "RUDPServer.h"

DUBU::Service::Service()
{
	rudpServer_ = std::make_shared<RUDPServer>();
}

DUBU::Service::~Service()
{
	rudpServer_->End();
}

void DUBU::Service::Initialize()
{
	rudpServer_->Start();
}
