#pragma once

#include <cstdint>

namespace apex {

// ------------------------------------------------------------------------------
// Fixed-width integer type aliases for better readability and consistency across platforms.
//
// The C++ standard guarantees these are exact-size types. Using short aliases improves code clarity and reduces verbosity.
// - u8: 8-bit unsigned integer (0 to 255)
// - u16: 16-bit unsigned integer (0 to 65,535)
// - u32: 32-bit unsigned integer (0 to 4,294,967,295)
// - u64: 64-bit unsigned integer (0 to 18,446,744,073,709,551,615)
// - i8: 8-bit signed integer (-128 to 127)
// - i16: 16-bit signed integer (-32,768 to 32,767)
// - i32: 32-bit signed integer (-2,147,483,648 to 2,147,483,647)
// - i64: 64-bit signed integer (-9,223,372,036,854,775,808 to 9,223,372,036,854,775,807)
// - f32: 32-bit floating-point number (single precision)
// - f64: 64-bit floating-point number (double precision)
// ------------------------------------------------------------------------------

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

// ------------------------------------------------------------------------------
// Engine-wide status codes for function return values.
//
// Returned from any function that can fail. Add new values as needed but never change existing values to avoid breaking existing code.
// -------------------------------------------------------------------------------
enum class Status : u32 {
    Ok = 0, // Operation completed successfully

    // General errors
    Unknown, // An unknown error occurred
    InvalidArgument, // An argument passed to a function was invalid
    OutOfMemory, // The system ran out of memory
    NotImplemented, // The requested functionality is not implemented
    
    // Platform Layer errors
    PlatformInitFailed, // Failed to initialize the platform layer
    WindowCreationFailed, // Failed to create a window

};

// Convienience function to check if a status code indicates success.
constexpr bool IsOk(Status s) {
    return s == Status::Ok;
}

} // namespace apex