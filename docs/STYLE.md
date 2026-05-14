# ApexEngine Code Style

This document captures the coding conventions used throughout ApexEngine.
The goal is consistency: every file should look like every other file, so
that reading the codebase doesn't require constantly recalibrating to a new
style.

Most of these rules are enforced automatically by `.clang-format` at the
project root. The few that aren't (naming conventions, comment content) are
on the developer.

---

## Language

- C++20.
- No exceptions (`-fno-exceptions`).
- No RTTI (`-fno-rtti`).
- Standard library functions that throw on bad input (e.g. `std::vector::at`,
  `std::stoi`) are forbidden. Use non-throwing equivalents.
- Allocation failures are reported with `nullptr`, not exceptions. Use
  `new(std::nothrow)` for all heap allocations.

## File layout

- Headers: `.h` extension. Sources: `.cpp` extension.
- Each header starts with `#pragma once`.
- Include order in source files:
  1. The file's own header (e.g. `#include "Window.h"` at the top of `Window.cpp`).
  2. Platform headers, with any required `#define`s before them.
  3. Standard library headers.
  4. Third-party headers (currently none).
- Includes within each group are grouped logically and may be alphabetical
  within a group but don't have to be.

## Namespaces

- All engine code lives in `namespace apex`.
- Subsystems may use nested namespaces if they become large enough to justify
  it; currently none do.
- File-local helpers go in an anonymous namespace inside the source file:

```cpp
  namespace apex {
  namespace {
      // file-private helpers here
  } // anonymous namespace
```

- `using namespace` directives are forbidden in headers. They may be used
  inside function bodies in source files when clarity benefits.

## Naming

| Kind                              | Convention            | Example                          |
|-----------------------------------|-----------------------|----------------------------------|
| Classes, structs, enums           | PascalCase            | `Window`, `LogLevel`             |
| Class member functions            | PascalCase            | `PollEvents`, `IsKeyDown`        |
| Free functions                    | PascalCase            | `TranslateVirtualKey`            |
| Enum class values                 | PascalCase            | `Key::Space`, `LogLevel::Info`   |
| Local variables and parameters    | camelCase             | `keyCode`, `windowWidth`         |
| Member variables                  | `m_` + camelCase      | `m_impl`, `m_keysDown`           |
| File-scope mutable state          | `g_` + camelCase      | `g_classRegistered`              |
| Class-scope static members        | `s_` + camelCase      | `s_instance`                     |
| Compile-time constants            | `k` + PascalCase      | `kWindowClassName`, `kKeyCount`  |
| Type aliases for fundamental types| lowercase short       | `u8`, `u32`, `i32`, `f32`        |
| Macros                            | UPPER_SNAKE_CASE      | `LOG_INFO`, `APEX_LOG_IMPL`      |
| Files                             | PascalCase            | `Window.h`, `Log.cpp`            |

Methods that ask about current state use `Is`/`Has`: `IsKeyDown`,
`IsMouseButtonDown`. Methods that ask about transitions this frame use
`Was`: `WasKeyPressed`, `WasKeyReleased`.

## Formatting

Most formatting is governed by `.clang-format`. Highlights:

- 4-space indentation, no tabs.
- Opening brace on the same line as the function/class/if/loop (K&R style).
- Pointer alignment: `int* p`, not `int *p`. Same for references: `int&`.
- One statement per line.
- Line length soft limit: 100 columns. Hard limit: 120 columns.
- Space inside parentheses: no. `if (condition)` not `if ( condition )`.
- Space before opening brace: yes. `if (x) {` not `if (x){`.
- Spaces around binary operators: yes. `a + b`, not `a+b`.

## Comments

- Use `//` for single-line and multi-line comments. `/* */` is reserved for
  rare cases where the comment must be inline with code mid-line.
- Block comments separating sections in a file use a row of dashes:

```cpp
  // ---------------------------------------------------------------------
  // Section name
  // ---------------------------------------------------------------------
```

- Doxygen comments use `///` triple-slash style:

```cpp
  /// @brief Translates a Win32 virtual key code to the engine's Key enum.
  /// @param vk Win32 virtual key code from WPARAM of WM_KEYDOWN/UP.
  /// @return Engine-native Key value, or Key::Unknown if unmapped.
```

- Document the *intent* of code, not the *mechanics*. `// Increment i` is
  noise; `// Skip auto-repeats — only fresh presses count` is content.

## Class design

- Public interface goes at the top, private implementation at the bottom.
- Constructors that can fail: use a static `Create()` factory returning a
  pointer (nullptr on failure), with a private constructor.
- Classes owning resources should explicitly delete copy/move operations
  unless they have a meaningful copy/move semantic.
- Use PIMPL (forward-declared `Impl` struct with pointer member) for any
  class whose implementation requires platform-specific headers.

## Singletons

Engine-wide subsystems (Log, Input) follow this pattern:

- Private constructor and destructor.
- Static `Init()`, `Shutdown()`, `Get()` methods.
- Static `s_instance` pointer holding the one instance.
- Init creates the instance via `new(std::nothrow)`, returns success/failure.
- Get returns `*s_instance`; caller is responsible for having called Init.
- Shutdown deletes the instance.

## Logging

Use the `LOG_*` macros, never `printf` or `std::cout` directly.

| Level   | Use for                                                   |
|---------|-----------------------------------------------------------|
| TRACE   | Extreme verbosity (per-frame state, hot-path tracing).    |
| DEBUG   | Per-event details useful when investigating an issue.     |
| INFO    | Major lifecycle events visible by default.                |
| WARN    | Unusual but recoverable conditions.                       |
| ERROR   | Operation failed but the program can continue.            |
| FATAL   | Unrecoverable. Program will abort.                        |

Format strings use `std::format`-style `{}` placeholders, not printf `%d`.

## Subsystem dependency order

When initializing subsystems in `main()`:

1. **Log** must be initialized first so other systems can log during their
   own initialization.
2. **Input** must be initialized before **Window** because Window's WndProc
   calls into Input during message dispatch.
3. **Shutdown is the reverse of init**, except: subsystems with dependents
   must be torn down before the systems they depend on. (Window must be
   destroyed before Input::Shutdown.)

## What's deferred

This style is for what exists today. The following are known future work
and will get their own conventions when implemented:

- Memory allocators (custom arena/pool/general allocators)
- Error type richer than `nullptr` (likely a `Status` or `std::expected`)
- Compile-time logging level filter for shipping builds
- Full UTF-8 / UTF-16 conversion helper
- Asserts (waiting on logging — implemented after the next milestone)