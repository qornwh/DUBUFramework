#include "CrashDump.h"
#include <DbgHelp.h>
#include <csignal>

#pragma comment(lib, "Dbghelp.lib")

void DUBU::CrashDump::Initialize()
{
    // 애플리케이션이 프로세스의 각 스레드에 대한 최상위 예외 처리기를 대체하도록 설정합니다.
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ex) -> LONG
        {
            WriteDump(ex);
            return EXCEPTION_EXECUTE_HANDLER;
        });

    // CRT가 잘못된 인수를 발견할 때 호출할 함수를 설정합니다.
    _set_invalid_parameter_handler([](const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
        {
            WriteDump(nullptr);
            TerminateProcess(GetCurrentProcess(), 1);
        });

    // 순수 가상 함수 호출에 대한 오류 처리기를 가져오거나 설정합니다.
    _set_purecall_handler([]()
        {
            WriteDump(nullptr);
            TerminateProcess(GetCurrentProcess(), 1);
        });

    // abort로 죽기 직전에 이 함수 먼저 콜
    signal(SIGABRT, [](int)
        {
            WriteDump(nullptr);
            TerminateProcess(GetCurrentProcess(), 1);
        });

    // abort() 함수로 비정상 종료될 때의 동작을 지정하는 마이크로소프트 C 런타임(CRT) 함수입니다.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT); // 오류 메시지나 팝업 대화상자를 출력할지 결정합니다. | : 윈도우 오류 보고(WER) 메커니즘을 호출할지 결정합니다.
}

void DUBU::CrashDump::WriteDump(EXCEPTION_POINTERS* exceptionInfo)
{
    static volatile LONG dumpInProgress = 0;

    // interlocked 계열로 동시성 방지
    if (InterlockedExchange(&dumpInProgress, 1) != 0)
    {
        Sleep(INFINITE);
    }

    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash != nullptr)
    {
        *(lastSlash + 1) = L'\0';
    }
    wcscat_s(path, L"Dumps");
    CreateDirectoryW(path, nullptr);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t filePath[MAX_PATH]{};
    swprintf_s(filePath, L"%s\\%04d%02d%02d_%02d%02d%02d_%u.dmp", path, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, GetCurrentProcessId());

    HANDLE file = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = exceptionInfo;
    mei.ClientPointers = FALSE;

    const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithThreadInfo | MiniDumpWithHandleData | MiniDumpWithIndirectlyReferencedMemory);
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, dumpType, (exceptionInfo != nullptr) ? &mei : nullptr, nullptr, nullptr);
    CloseHandle(file);
}