#include "Assert.h"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#include <windows.h> // for IsDebuggerPresent, OutputDebugStringA
#endif

namespace apex::detail {

bool IsDebuggerAttached() {
#ifdef _WIN32
    return IsDebuggerPresent() != FALSE;
#else
    // Linux: there is no clean POSIX way to detect a debugger. We could
    // parse /proc/self/status for TracerPid, but for now we just say
    // "no" and rely on std::abort() to make the failure obvious.
    return false;
#endif
}

[[noreturn]] void AssertFail(const char* condition,
                             const char* file,
                             int line,
                             std::string_view message) {
    char buffer[2048];
    if (message.empty()) {
        std::snprintf(buffer,
                      sizeof(buffer),
                      "\n*** ASSERTION FAILED ***\n"
                      "  Condition: %s\n"
                      "  Location:  %s:%d\n\n",
                      condition,
                      file,
                      line);
    } else {
        std::snprintf(buffer,
                      sizeof(buffer),
                      "\n*** ASSERTION FAILED ***\n"
                      "  Condition: %s\n"
                      "  Location:  %s:%d\n"
                      "  Message:   %.*s\n\n",
                      condition,
                      file,
                      line,
                      static_cast<int>(message.size()),
                      message.data());
    }

    std::fputs(buffer, stderr);
    std::fflush(stderr);

#ifdef _WIN32
    OutputDebugStringA(buffer);
#endif

    if (IsDebuggerAttached()) {
#ifdef _WIN32
        __debugbreak();
#else
        __builtin_trap();
#endif
    }

    // Unconditional abort so this function is truly [[noreturn]] even if
    // a debugger lets execution continue past __debugbreak().
    std::abort();
}

} // namespace apex::detail
