#pragma once

#include "Common.h"

#include <format>
#include <string>
#include <string_view>

/// @file Log.h
/// @brief Logging system with severity levels, per-category thresholds,
/// and console + file + debugger sinks.

namespace apex {

/// Logging severity levels, in ascending order.
///
/// @ref Off is a threshold-only sentinel — it is never used as the level
/// of an emitted message, only as the threshold value that suppresses
/// every message in a category.
enum class LogLevel : u32 {
    Trace = 0, ///< Most verbose. Fine-grained tracing for deep debugging.
    Debug,     ///< Verbose diagnostic detail useful during development.
    Info,      ///< Informational messages about normal operation.
    Warn,      ///< Something unexpected happened, but execution continues.
    Error,     ///< A recoverable error. The operation failed but the program continues.
    Fatal,     ///< Unrecoverable error. Logging this aborts the program.
    Off,       ///< Sentinel threshold meaning "emit nothing." Never used as a message level.
};

/// Singleton logging system.
///
/// Three sinks are written on every emitted message:
///   - stdout, colorized via ANSI escape codes
///   - `OutputDebugStringA` (for the debugger's output window on Windows)
///   - A timestamped log file at `logs/apex_YYYY-MM-DD_HH-MM-SS.log`
///
/// **Lifecycle:**
/// @code
/// apex::Log::Init();
/// // ... use LOG_* macros ...
/// apex::Log::Shutdown();
/// @endcode
///
/// Direct calls into Log are rare — engine and game code use the
/// @ref LOG_TRACE / @ref LOG_DEBUG / @ref LOG_INFO / @ref LOG_WARN /
/// @ref LOG_ERROR / @ref LOG_FATAL macros, which capture `__FILE__` and
/// `__LINE__` at the call site and skip formatting work when the message
/// would be filtered out.
class Log {
public:
    // ---- Lifecycle ----

    /// Initialize the logger. Call once at program start.
    /// @return true on success; false if allocation failed.
    static bool Init();

    /// Tear down the logger, flushing and closing the log file.
    static void Shutdown();

    /// Access the singleton instance.
    /// @pre @ref Init has been called and not followed by @ref Shutdown.
    static Log& Get();

    /// @cond
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log(Log&&) = delete;
    Log& operator=(Log&&) = delete;
    /// @endcond

    // ---- Configuration ----

    /// Set the default level threshold. Categories without an explicit
    /// per-category threshold use this.
    void SetGlobalLevel(LogLevel level);

    /// Set the threshold for a specific category. Messages logged to
    /// @p category with severity below @p level are dropped.
    void SetCategoryLevel(std::string_view category, LogLevel level);

    // ---- Query ----

    /// Fast check whether a message at this level/category would be
    /// emitted. The LOG_* macros call this before any formatting work so
    /// that disabled messages cost only a hash-map lookup.
    bool IsEnabled(LogLevel level, std::string_view category) const;

    // ---- Emit (called by the LOG_* macros, not directly) ----

    /// Emit an already-formatted message to all sinks.
    ///
    /// @param level    Severity of this message.
    /// @param category Category the message belongs to.
    /// @param file     Source file (`__FILE__`) of the call site.
    /// @param line     Source line (`__LINE__`) of the call site.
    /// @param message  The pre-formatted message string.
    ///
    /// @note If @p level is @ref LogLevel::Fatal, this function flushes
    /// the log file and calls `std::abort()` after emitting.
    void Emit(LogLevel level,
              std::string_view category,
              const char* file,
              int line,
              std::string_view message);

private:
    Log() = default;
    ~Log() = default;

    static Log* s_instance;

    // Implementation state lives in Log.cpp inside an anonymous namespace.
    // This keeps <mutex>, <fstream>, <unordered_map>, etc. out of Log.h
    // so every translation unit that includes Log.h doesn't pay for
    // parsing those headers.
};

} // namespace apex

// ---------------------------------------------------------------------------
// Logging macros.
//
// Each macro:
//   1. Checks if the level/category is enabled (cheap hash-map lookup).
//   2. If so, formats the message via std::format.
//   3. Calls Log::Get().Emit() with __FILE__/__LINE__.
//
// Format strings must be string literals so std::format can check them
// at compile time. The engine is built with -fno-exceptions, so a bad
// format string with a non-literal source would be undefined behavior.
// ---------------------------------------------------------------------------

/// @cond INTERNAL
#define APEX_LOG_IMPL(level, category, ...)                                                        \
    do {                                                                                           \
        if (::apex::Log::Get().IsEnabled((level), (category))) {                                   \
            ::apex::Log::Get().Emit(                                                               \
                (level), (category), __FILE__, __LINE__, std::format(__VA_ARGS__));                \
        }                                                                                          \
    } while (0)
/// @endcond

/// Emit a Trace-level log message. @see apex::LogLevel::Trace
#define LOG_TRACE(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Trace, (category), __VA_ARGS__)

/// Emit a Debug-level log message. @see apex::LogLevel::Debug
#define LOG_DEBUG(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Debug, (category), __VA_ARGS__)

/// Emit an Info-level log message. @see apex::LogLevel::Info
#define LOG_INFO(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Info, (category), __VA_ARGS__)

/// Emit a Warn-level log message. @see apex::LogLevel::Warn
#define LOG_WARN(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Warn, (category), __VA_ARGS__)

/// Emit an Error-level log message. @see apex::LogLevel::Error
#define LOG_ERROR(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Error, (category), __VA_ARGS__)

/// Emit a Fatal-level log message, then abort the program.
/// @see apex::LogLevel::Fatal
#define LOG_FATAL(category, ...) APEX_LOG_IMPL(::apex::LogLevel::Fatal, (category), __VA_ARGS__)
