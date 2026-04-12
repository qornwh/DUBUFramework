#pragma once
#include "pch.h"

namespace DUBU
{
    // Session 전방선언
    class Session;

    struct Peer
    {
        Uint64 key_;
        SOCKADDR_IN addr_;
        Session* session_;
    };
}