#pragma once
#include <atomic>
#include <string>

namespace Synapse::Console {
    /**
     * @brief A global atomic flag used to manage the state of the console execution.
     *
     * This variable serves as a signal to control the console's running state. It is
     * primarily used to detect and handle interruptions or termination signals
     * (e.g., Ctrl+C) in a platform-independent manner.
     *
     * When set, the console is in a "running" state. Interrupt handlers or other
     * shutdown mechanisms clear this flag to indicate that the console should
     * terminate its operations gracefully.
     *
     * Thread-safe and suitable for use in multithreaded environments.
     */
    inline std::atomic_flag console_running{};

    auto ChangeConsoleTitle(std::string title) -> void;
    auto SetConsoleControls() -> bool;
}
