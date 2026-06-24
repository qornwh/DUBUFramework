#pragma once
#include "Subheader.h"

// 2번 타입으로 설정
struct GatewaySubHeader : public DUBU::Packet::Subheader<GatewaySubHeader, 2>
{
    // 초기 클라이언트에서 게이트웨이로 생성된 세션 id
    Uint32 depthId1 = 0;

    // 게이트웨이에서 내부서버로 생성된 세션 id
    Uint32 depthId2 = 0;
};

