#include "pch.h"

void DUBU::LogAsset(const char* str, bool ret)
{
    if (!ret)
    {
        printf("%s", str);
        assert(ret);
    }
}
Uint64 DUBU::GetCurrentTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
Uint32 DUBU::GetRelativeTimeMs()
{
    static const auto start = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::steady_clock::now() - start;
    return static_cast<Uint32>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}
