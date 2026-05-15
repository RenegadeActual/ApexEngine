#include "Log.h"

#include "Assert.h"

#define _WIN32_WINNT 0x0A00
#include <windows.h> // for OutputDebugStringA and console color setup

#include <cstdio>
#include <cstdlib>    // for std::abort
#include <ctime>      // for time, localtime_s, strftime
#include <filesystem> // for std::filesystem::create_directories
#include <fstream>    // for std::ofstream
#include <mutex>
#include <new>
#include <unordered_map>

namespace apex {

// ---------------------------------------------------------------------------
// Out-of-class definition for the singleton pointer.
// ---------------------------------------------------------------------------
Log* Log::s_instance = nullptr;

// ---------------------------------------------------------------------------
// File-scope state.
// We don't put these in the Log class header to keep <fstream>, <mutex>,
// <unordered_map>, etc. out of Log.h. Every translation unit that includes
// Log.h would otherwise pay for parsing those headers.
// ---------------------------------------------------------------------------
namespace {

struct LogState {
    std::mutex mutex;
    std::ofstream file;
    LogLevel globalLevel = LogLevel::Info;
    std::unordered_map<std::string, LogLevel> categoryLevels;
};

LogState* g_state = nullptr;

// ANSI color escape codes for severity levels in the console.
const char* LevelColor(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "\033[90m"; // bright black (grey)
    case LogLevel::Debug:
        return "\033[36m"; // cyan
    case LogLevel::Info:
        return "\033[37m"; // white
    case LogLevel::Warn:
        return "\033[33m"; // yellow
    case LogLevel::Error:
        return "\033[31m"; // red
    case LogLevel::Fatal:
        return "\033[91m"; // bright red
    default:
        return "\033[0m"; // reset
    }
}

const char* LevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO ";
    case LogLevel::Warn:
        return "WARN ";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    default:
        return "?????";
    }
}

// ANSI reset code.
const char* kResetColor = "\033[0m";

// Enable ANSI escape code processing on the Windows console.
// Without this, our color codes would just print as literal text.
void EnableConsoleColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
        return;
    }
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) {
        return;
    }
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
}

// Build a timestamped log filename: logs/apex_YYYY-MM-DD_HH-MM-SS.log
// Uses C-style time functions because std::chrono format support is
// uneven across compilers.
std::string MakeLogFilename() {
    std::time_t now = std::time(nullptr);
    std::tm tm_local = {};
#ifdef _WIN32
    localtime_s(&tm_local, &now);
#else
    localtime_r(&now, &tm_local);
#endif

    char buffer[128] = {};
    std::strftime(buffer, sizeof(buffer), "logs/apex_%Y-%m-%d_%H-%M-%S.log", &tm_local);
    return std::string(buffer);
}

// Build the timestamp prefix that appears on each log line: [HH:MM:SS.mmm]
std::string MakeTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_local = {};
#ifdef _WIN32
    localtime_s(&tm_local, &now_t);
#else
    localtime_r(&now_t, &tm_local);
#endif

    char buffer[32] = {};
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%02d:%02d:%02d.%03d",
                  tm_local.tm_hour,
                  tm_local.tm_min,
                  tm_local.tm_sec,
                  static_cast<int>(ms.count()));
    return std::string(buffer);
}

// Extract just the filename from a full path. __FILE__ gives us the full
// path (long and noisy); we want just "Window.cpp" not the full E:\... path.
const char* BaseName(const char* path) {
    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Log class implementation.
// ---------------------------------------------------------------------------

bool Log::Init() {
    if (s_instance != nullptr) {
        return true;
    }

    s_instance = new (std::nothrow) Log();
    if (s_instance == nullptr) {
        return false;
    }

    g_state = new (std::nothrow) LogState();
    if (g_state == nullptr) {
        delete s_instance;
        s_instance = nullptr;
        return false;
    }

    // Create logs/ directory if it doesn't exist.
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    if (ec) {
        // Non-fatal: we'll just lose file logging if this fails.
        std::fprintf(stderr, "Log: failed to create logs/ directory: %s\n", ec.message().c_str());
    }

    // Open the log file.
    std::string filename = MakeLogFilename();
    g_state->file.open(filename, std::ios::out | std::ios::trunc);
    if (!g_state->file.is_open()) {
        std::fprintf(stderr, "Log: failed to open log file '%s'\n", filename.c_str());
        // Still return true — we can log to console even without file output.
    }

    EnableConsoleColors();

    return true;
}

void Log::Shutdown() {
    if (g_state != nullptr) {
        if (g_state->file.is_open()) {
            g_state->file.flush();
            g_state->file.close();
        }
        delete g_state;
        g_state = nullptr;
    }
    delete s_instance;
    s_instance = nullptr;
}

Log& Log::Get() {
    APEX_ASSERT(s_instance != nullptr, "Log::Get() called before Log::Init()");
    return *s_instance;
}

void Log::SetGlobalLevel(LogLevel level) {
    if (g_state == nullptr)
        return;
    std::lock_guard<std::mutex> lock(g_state->mutex);
    g_state->globalLevel = level;
}

void Log::SetCategoryLevel(std::string_view category, LogLevel level) {
    if (g_state == nullptr)
        return;
    std::lock_guard<std::mutex> lock(g_state->mutex);
    g_state->categoryLevels[std::string(category)] = level;
}

bool Log::IsEnabled(LogLevel level, std::string_view category) const {
    if (g_state == nullptr)
        return false;

    // Per-category level overrides global level if set.
    // We do a hash-map lookup without locking for performance; in practice
    // category levels rarely change after startup. If they do change at
    // runtime, a torn read is benign (worst case: one message slips through
    // or gets dropped right at the moment of change).
    auto it = g_state->categoryLevels.find(std::string(category));
    LogLevel threshold = (it != g_state->categoryLevels.end()) ? it->second : g_state->globalLevel;

    return static_cast<u32>(level) >= static_cast<u32>(threshold);
}

void Log::Emit(LogLevel level,
               std::string_view category,
               const char* file,
               int line,
               std::string_view message) {
    if (g_state == nullptr)
        return;

    std::lock_guard<std::mutex> lock(g_state->mutex);

    const std::string timestamp = MakeTimestamp();
    const char* levelName = LevelName(level);
    const char* fileName = BaseName(file);

    // Console output (with colors).
    std::fprintf(stdout,
                 "%s[%s] [%s] [%s] %.*s%s (%s:%d)\n",
                 LevelColor(level),
                 timestamp.c_str(),
                 levelName,
                 std::string(category).c_str(),
                 static_cast<int>(message.size()),
                 message.data(),
                 kResetColor,
                 fileName,
                 line);
    std::fflush(stdout);

    // Also write to OutputDebugString so the message shows up in the
    // VS Code / debugger output window when running under a debugger.
    // No colors; that channel doesn't understand them.
    {
        char debugBuffer[1024];
        std::snprintf(debugBuffer,
                      sizeof(debugBuffer),
                      "[%s] [%s] [%s] %.*s (%s:%d)\n",
                      timestamp.c_str(),
                      levelName,
                      std::string(category).c_str(),
                      static_cast<int>(message.size()),
                      message.data(),
                      fileName,
                      line);
        OutputDebugStringA(debugBuffer);
    }

    // File output (no colors).
    if (g_state->file.is_open()) {
        g_state->file << "[" << timestamp << "] "
                      << "[" << levelName << "] "
                      << "[" << category << "] " << message << " (" << fileName << ":" << line
                      << ")\n";
        g_state->file.flush();
    }

    // Fatal logs terminate the program after emitting.
    if (level == LogLevel::Fatal) {
        if (g_state->file.is_open()) {
            g_state->file.flush();
            g_state->file.close();
        }
        std::abort();
    }
}

} // namespace apex