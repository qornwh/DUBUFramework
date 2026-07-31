#pragma once
#include "pch.h"

namespace DUBU
{
    // 왜 아직도 C++은 덤프를 코드를 짜야 하는가 .....
    class CrashDump
    {
    public:
        static void Initialize();

    private:
        static void WriteDump(EXCEPTION_POINTERS* exceptionInfo);
    };
}