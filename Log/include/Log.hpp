#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

/**
 * @namespace Synapse::Log
 * @brief Core logging facilities for the Synapse framework.
 *
 * This namespace wraps the framework-wide logger and provides a set of
 * convenience macros for logging at various severity levels.
 */
namespace Synapse::Log {
    /**
     * @class Log
     * @brief Static interface to the core logger.
     *
     * This class owns and manages the core spdlog logger instance used
     * throughout the framework. It cannot be instantiated and is intended
     * to be used purely via its static member functions.
     */
    class Log {
    public:
        /**
         * @brief Initialises the logging system.
         *
         * Sets up the core logger instance and configures logging sinks
         * (e.g., console, file). This function must be called once before
         * any logging macros or GetCoreLogger() are used.
         *
         * @param console_log_on If true, log output is also sent to the console.
         */
        static auto Initialise(bool console_log_on = true) -> void;

        /**
         * @brief Returns a reference to the core logger.
         *
         * Provides access to the underlying spdlog logger used by the framework.
         * The returned shared pointer remains owned by the Log class and
         * should not be reset or replaced by callers.
         *
         * @return Reference to a shared pointer to the core logger.
         */
        static auto GetCoreLogger() -> std::shared_ptr<spdlog::logger> & { return core_logger; }

        /// Deleted copy constructor.
        Log(const Log&) = delete;
        /// Deleted move constructor.
        Log(Log&&) = delete;
        /// Deleted copy assignment operator.
        auto operator=(const Log &) -> Log & = delete;
        /// Deleted move assignment operator.
        auto operator=(Log &&) -> Log & = delete;

        /**
         * @brief Crash signal handler.
         *
         * Handles fatal signals (e.g., segmentation faults) by logging
         * relevant diagnostic information and performing any last-chance
         * cleanup or flushing of log buffers.
         *
         * @param sig Signal number that caused the handler to be invoked.
         */
        static auto CrashHandler(int sig) -> void;

    private:
        /// Private default constructor to prevent instantiation
        Log() = default;

        /// Private destructor to prevent deletion via pointer
        ~Log() = default;

        /**
         * @brief Initialises the crash handler.
         *
         * Registers CrashHandler() with the runtime so that it is invoked
         * when fatal signals are raised.
         */
        static auto InitialiseCrashHandler() -> void;

        /// Core spdlog logger instance shared across the framework.
        static std::shared_ptr<spdlog::logger> core_logger;
    };

    /**
     * @def CORE_DEBUG
     * @brief Logs a debug-level message to the core logger (only in debug builds).
     *
     * This macro forwards its arguments to spdlog::logger::debug().
     */
#ifdef _DEBUG
#define CORE_DEBUG(...) Synapse::Log::Log::GetCoreLogger()->debug(__VA_ARGS__)
#else
#define CORE_DEBUG(...)
#endif

    /**
     * @def CORE_TRACE
     * @brief Logs a trace-level message to the core logger.
     *
     * This macro forwards its arguments to spdlog::logger::trace().
     */
#define CORE_TRACE(...) Synapse::Log::Log::GetCoreLogger()->trace(__VA_ARGS__)

    /**
     * @def CORE_INFO
     * @brief Logs an info-level message to the core logger.
     *
     * This macro forwards its arguments to spdlog::logger::info().
     */
#define CORE_INFO(...) Synapse::Log::Log::GetCoreLogger()->info(__VA_ARGS__)

    /**
     * @def CORE_WARN
     * @brief Logs a warning-level message to the core logger.
     *
     * This macro forwards its arguments to spdlog::logger::warn().
     */
#define CORE_WARN(...) Synapse::Log::Log::GetCoreLogger()->warn(__VA_ARGS__)

    /**
     * @def CORE_ERROR
     * @brief Logs an error-level message to the core logger.
     *
     * This macro forwards its arguments to spdlog::logger::error().
     */
#define CORE_ERROR(...) Synapse::Log::Log::GetCoreLogger()->error(__VA_ARGS__)

    /**
     * @def CORE_CRITICAL
     * @brief Logs a critical-level message to the core logger.
     *
     * This macro forwards its arguments to spdlog::logger::critical().
     */
#define CORE_CRITICAL(...) Synapse::Log::Log::GetCoreLogger()->critical(__VA_ARGS__)
} // namespace Synapse::Log
