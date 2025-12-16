#include <FileUtils.hpp>
#include <Log.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <csignal>
#include <vector>
#include <cpptrace/cpptrace.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Synapse::Log {
    std::shared_ptr<spdlog::logger> Log::core_logger;

    auto Log::Initialise(bool console_log_on) -> void {
        (void)std::filesystem::remove_all(FileSystem::GetAbsoluteExecutableDirectory() / "tempfiles");
        (void)std::filesystem::create_directory(FileSystem::GetAbsoluteExecutableDirectory() / "tempfiles");

        std::vector<spdlog::sink_ptr> log_sinks;
        if (console_log_on) {
            (void)log_sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>())->set_pattern("%^[%T] %n: %v%$");
        }

        const std::chrono::time_point now{ std::chrono::system_clock::now() };
        const std::chrono::year_month_day ymd{ std::chrono::floor<std::chrono::days>(now) };
        log_sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                (FileSystem::GetAbsoluteExecutableDirectory() / "tempfiles" / std::format("LOG.{}-{}-{}", ymd.day(), ymd.month(), ymd.year())).string(), true))->set_pattern("[%T] [%l] %n: %v");

        core_logger = std::make_shared<spdlog::logger>("CORE", begin(log_sinks), end(log_sinks));
        spdlog::register_logger(core_logger);

        core_logger->set_level(spdlog::level::trace);
        core_logger->flush_on(spdlog::level::trace);

        InitialiseCrashHandler();
    }

#ifdef WIN32
    auto Log::CrashHandler([[maybe_unused]] int sig) -> void {
        // Generate trace
        std::ofstream crash_file;
        const std::chrono::time_point now{ std::chrono::system_clock::now() };
        const std::chrono::year_month_day ymd{ std::chrono::floor<std::chrono::days>(now) };
        crash_file.open(FileSystem::GetAbsoluteExecutableDirectory() / "tempfiles" / std::format("CRASH.{}-{}-{}", ymd.day(), ymd.month(), ymd.year()), std::ios::binary | std::ios::in | std::ios::trunc);
        constexpr std::size_t n = 100U;
        cpptrace::frame_ptr buffer[n]{};
        const std::size_t count = cpptrace::safe_generate_raw_trace(buffer, n);
        for (std::size_t i = 0U; i < count; i++) {
            cpptrace::safe_object_frame frame{};
            cpptrace::get_safe_object_frame(buffer[i], &frame);
            (void)crash_file.write(std::bit_cast<char*>(&frame), sizeof(frame));
        }

        std::terminate();
    }
#else
#endif
    auto Log::InitialiseCrashHandler() -> void {
        cpptrace::frame_ptr buffer[10]{};
        (void)cpptrace::safe_generate_raw_trace(buffer, 10U);
        cpptrace::safe_object_frame frame{};
        cpptrace::get_safe_object_frame(buffer[0], &frame);

#ifdef WIN32
        (void)signal(SIGSEGV, &Log::CrashHandler);
        (void)signal(SIGABRT, &Log::CrashHandler);
#else
            struct sigaction action = { 0 };
            action.sa_flags = 0;
            action.sa_sigaction = &CrashHandler;
            if (sigaction(SIGSEGV, &action, NULL) == -1) {
                perror("sigaction");
                std::terminate()
            }
            if (sigaction(SIGABRT, &action, NULL) == -1) {
                perror("sigaction");
                std::terminate()
            }
#endif
    }
}
