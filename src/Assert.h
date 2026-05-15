#pragma once

#include "Common.h"

#include <format>
#include <string>
#include <string_view>

/// @file Assert.h
/// @brief Runtime assertion macros for catching invariant violations.
///
/// Assertions are debug-only checks for conditions that should never be
/// false if the code is correct. They are stripped entirely in release
/// builds (when `NDEBUG` is defined), so they have zero cost in shipped
/// binaries.
///
/// Use @ref APEX_ASSERT for plain checks, @ref APEX_VERIFY when the
/// checked expression has side effects that must execute even in
/// release, and @ref APEX_UNREACHABLE for code paths that the program
/// must never enter (e.g. a `default:` case that handles every enum
/// value above it).

namespace apex::detail {

/// Returns true if a debugger is currently attached to the process.
bool IsDebuggerAttached();

/// Called by the assertion macros when a check fails.
///
/// Writes a formatted failure message to `stderr` and the debugger's
/// output channel, then breaks into the debugger (if attached) or
/// aborts the process.
///
/// @param condition Stringified expression that evaluated to false.
/// @param file      Source file of the call site (`__FILE__`).
/// @param line      Source line of the call site (`__LINE__`).
/// @param message   Optional caller-supplied message. May be empty.
[[noreturn]] void AssertFail(const char* condition,
                             const char* file,
                             int line,
                             std::string_view message);

/// Format an optional std::format-style message for an assertion macro.
/// Used internally by @ref APEX_ASSERT; not intended for direct use.
template <typename... Args>
std::string FormatAssertMessage(std::format_string<Args...> fmt, Args&&... args) {
    return std::format(fmt, std::forward<Args>(args)...);
}

/// Zero-argument overload for assertions without a message.
inline std::string FormatAssertMessage() {
    return {};
}

} // namespace apex::detail

#ifdef NDEBUG

/// @brief Assert that @p cond is true. Stripped in release builds.
///
/// Optional std::format-style message:
/// @code
/// APEX_ASSERT(index < count);
/// APEX_ASSERT(ptr != nullptr, "Subsystem not initialized");
/// APEX_ASSERT(value >= 0, "Bad value: got {}", value);
/// @endcode
#define APEX_ASSERT(cond, ...) ((void)0)

/// @brief Like @ref APEX_ASSERT, but always evaluates @p cond.
///
/// Use this when the checked expression has side effects that must run
/// in release builds too. In release, the result is evaluated and
/// discarded; in debug, it is also checked.
#define APEX_VERIFY(cond, ...) ((void)(cond))

/// @brief Marks a code path as logically unreachable.
///
/// In debug, firing this is an assertion failure. In release, it is a
/// compiler hint that the code is dead — the optimizer may remove
/// surrounding branches.
#define APEX_UNREACHABLE() __builtin_unreachable()

#else

#define APEX_ASSERT(cond, ...)                                                                     \
    do {                                                                                           \
        if (!(cond)) [[unlikely]] {                                                                \
            ::apex::detail::AssertFail(                                                            \
                #cond, __FILE__, __LINE__, ::apex::detail::FormatAssertMessage(__VA_ARGS__));      \
        }                                                                                          \
    } while (0)

#define APEX_VERIFY(cond, ...) APEX_ASSERT(cond, __VA_ARGS__)

#define APEX_UNREACHABLE()                                                                         \
    ::apex::detail::AssertFail("APEX_UNREACHABLE", __FILE__, __LINE__, std::string{})

#endif
