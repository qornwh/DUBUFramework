#include "pch.h"
#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <ctime>
#include <cstdio>

namespace DUBU
{
    void InitFileLog(const String& tag)
    {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);

        char name[128];
        std::snprintf(name, sizeof(name), "stresstest_%04d%02d%02d_%02d%02d%02d_%s.log",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,tm.tm_hour, tm.tm_min, tm.tm_sec, tag.c_str());

        auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(name, true);

        spdlog::set_default_logger(std::make_shared<spdlog::logger>("dubu", spdlog::sinks_init_list{ console, file }));

        // Ctrl+C 로 꺼도 버퍼가 안 날아가게
        spdlog::flush_on(spdlog::level::info);
        spdlog::info("log file : {}", name);
    }
}