#if defined(__linux__) || defined(__unix__)
#include <stdio.h>
#elif defined(_WIN32)
#define NOMINMAX
#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <Console.hpp>
#include <Log.hpp>

namespace Synapse::Console {
#ifdef _WIN32
    auto WINAPI Interrupt_Handler(const DWORD type) -> BOOL {
        switch (type) {
            case CTRL_C_EVENT:
                console_running.clear();
                break;
            case CTRL_BREAK_EVENT:
                console_running.clear();
                break;
            default:
                break;
        }
        return TRUE;
    }
#elif defined(__linux__) || defined(__unix__)
        void interrupt_handler([[maybe_unused]] int signal) {
            keep_running.clear();
        }
#endif

    auto SetConsoleControls() -> bool {
        CORE_INFO("Please close this application using Ctrl+C to avoid data loss.");
#ifdef _WIN32
        if (!SetConsoleCtrlHandler(static_cast<PHANDLER_ROUTINE>(Interrupt_Handler), TRUE)) {
            return false;
        }
#elif defined(__linux__) || defined(__unix__)
        if (signal(SIGINT, interrupt_handler) == SIG_ERR) {
            return false;
        }
#endif
        return true;
    }


    auto ChangeConsoleTitle(std::string title) -> void {
#if defined(WIN32)
        (void)SetConsoleTitleA(title.c_str());
#elif __unix__
            printf("%c]0;%s%c", '\033', title, '\007');
#endif
    }
}
