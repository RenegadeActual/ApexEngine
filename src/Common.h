#pragma once

#include <cstdint>

/// @file Common.h
/// @brief Engine-wide primitive type aliases and status codes.

namespace apex {

// ---------------------------------------------------------------------------
// Fixed-width integer and floating-point type aliases.
//
// These short names are used throughout the engine in place of
// std::uint32_t, etc. They make declarations less verbose and signal
// "engine type" at a glance. The C++ standard guarantees exact-size
// representations.
// ---------------------------------------------------------------------------

using u8 = std::uint8_t;    ///< 8-bit unsigned integer  (0 .. 255).
using u16 = std::uint16_t;  ///< 16-bit unsigned integer (0 .. 65,535).
using u32 = std::uint32_t;  ///< 32-bit unsigned integer (0 .. 4,294,967,295).
using u64 = std::uint64_t;  ///< 64-bit unsigned integer (0 .. 18,446,744,073,709,551,615).

using i8 = std::int8_t;     ///< 8-bit signed integer  (-128 .. 127).
using i16 = std::int16_t;   ///< 16-bit signed integer (-32,768 .. 32,767).
using i32 = std::int32_t;   ///< 32-bit signed integer (-2,147,483,648 .. 2,147,483,647).
using i64 = std::int64_t;   ///< 64-bit signed integer (full int64 range).

using f32 = float;   ///< 32-bit IEEE-754 single-precision floating-point.
using f64 = double;  ///< 64-bit IEEE-754 double-precision floating-point.

/// Engine-wide status codes for function return values.
///
/// Returned from any function that can fail. Add new values as needed,
/// but never change the integer value of an existing entry — code may
/// depend on specific representations.
enum class Status : u32 {
    Ok = 0, ///< Operation completed successfully.

    // General errors
    Unknown,         ///< An unknown error occurred.
    InvalidArgument, ///< An argument passed to a function was invalid.
    OutOfMemory,     ///< Memory allocation failed.
    NotImplemented,  ///< The requested functionality is not implemented.

    // Platform Layer errors
    PlatformInitFailed,   ///< Failed to initialize the platform layer.
    WindowCreationFailed, ///< Failed to create an OS window.

};

/// Returns true if @p s indicates success.
constexpr bool IsOk(Status s) {
    return s == Status::Ok;
}

} // namespace apex
