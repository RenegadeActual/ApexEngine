#pragma once

#include "Common.h"

#include <format>
#include <string>
#include <string_view>

namespace apex {

// ---------------------------------------------------------------------------
// Severity levels in ascending order. Off is a sentinel meaning "never emit"
// and is used when configuring per-category thresholds.
// ---------------------------------------------------------------------------
enum class LogLevel : u32 {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Off, // Sentinel; not a value you'd ever log AT, only set AS a threshold.
};

// ---------------------------------------------------------------------------
// Singleton logging system.
//
// Lifecycle:
//   Log::Init();
//   ... use LOG_* macros ...
//   Log::Shutdown();
//
// Direct usage of methods is rare — game and engine code use the LOG_TRACE,
// LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL macros below.
// ---------------------------------------------------------------------------
class Log {
public:
    // ---- Lifecycle ----
    static bool Init();
    static void Shutdown();
    static Log& Get();

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log(Log&&) = delete;
    Log& operator=(Log&&) = delete;

    // ---- Configuration ----

    // The global default level. Categories without a specific level use this.
    void SetGlobalLevel(LogLevel level);

    // Per-category threshold. Messages logged to `category` with severity
    // below `level` are skipped.
    void SetCategoryLevel(std::string_view category, LogLevel level);

    // ---- Query ----

    // Fast check whether a message at this level/category would be emitted.
    // Used by the LOG_* macros to skip formatting work when disabled.
    bool IsEnabled(LogLevel level, std::string_view category) const;

    // ---- Emit (called by the LOG_* macros, not directly) ----

    // Emit an already-formatted message. The macros call this after
    // formatting the message via std::format.
    void Emit(LogLevel level,
              std::string_view category,
              const char* file,
              int line,
              std::string_view message);

private:
    Log() = default;
    ~Log() = default;

    static Log* s_instance;

    // Implementation state is hidden in the .cpp (PIMPL-ish).
    // We can't forward-declare it cleanly here because Log can't have a
    // pointer member without committing to the type; we'll just declare
    // the methods and define the state inside Log.cpp via file statics.
};

} // namespace apex

// ---------------------------------------------------------------------------
// Logging macros.
//
// Each macro:
//   1. Checks if the level/category is enabled (cheap).
//   2. If so, formats the message via std::format.
//   3. Calls Log::Get().Emit() with file/line info.
//
// Format strings must be string literals so std::format can check them at
// compile time. We don't have exception support; a bad format string with
// non-literal source would be undefined behavior.
// ---------------------------------------------------------------------------

#define APEX_LOG_IMPL(level, category, ...)                                                        \
    do {                                                                                           \
        if (::apex::Log::Get().IsEnabled((level), (category))) {                                   \
            ::apex::Log::Get().Emit(                                                               \
                (level), (category), __FILE__, __LINE__, std::format(__VA_ARGS__));                \
        }                                                                                          \
    } while (0)

#define LOG_TRACE(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Trace, (category), __VA_ARGS__)
#define LOG_DEBUG(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Debug, (category), __VA_ARGS__)
#define LOG_INFO(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Info, (category), __VA_ARGS__)
#define LOG_WARN(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Warn, (category), __VA_ARGS__)
#define LOG_ERROR(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Error, (category), __VA_ARGS__)
#define LOG_FATAL(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Fatal, (category), __VA_ARGS__)