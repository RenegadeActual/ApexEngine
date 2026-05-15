# ApexEngine

A custom 3D engine being built from scratch in C++. Windows only, support for Linux planned for the future.
No console or mobile targets planned.

This is a solo project in active early development. Don't expect a usable
engine yet; expect a real-time logbook of what gets built and how.

## Documentation

API reference (Doxygen): https://resume.whatley3.com/ApexEngine/

Docs rebuild automatically on every push to `main` via GitHub Actions.

## Status

Currently has:

- Cross-platform foundation (Windows and Linux as targets; only Windows
  implemented so far)
- Win32 window abstraction with proper UNICODE handling
- Keyboard and mouse input system (polled, edge-detected, with auto-repeat
  filtering and focus-loss recovery)
- Logging system with severity levels, per-category thresholds, ANSI-colored
  console output, debugger output, and timestamped log files

Not yet:

- Linux platform layer
- Vulkan renderer
- Anything to do with actual game content

## Building

Requires:

- A C++20 compiler (clang is the primary supported toolchain)
- CMake 3.25 or newer
- Ninja
- The Vulkan SDK (currently unused; will be required once the renderer
  lands)

On Windows, the Visual Studio Build Tools provide the Windows SDK and
C++ standard library that clang uses by default.

To build:

    cmake --preset windows-debug
    cmake --build --preset windows-debug

The executable is at `build/windows-debug/apex.exe`. Run it from a
terminal; logs appear on the console and in a timestamped file under
`logs/`.

## Repository layout

    src/
        Common.h          Engine-wide type aliases (u32, f32, etc.) and Status enum
        Window.h/cpp      Platform window abstraction (Win32 implementation)
        Input.h/cpp       Keyboard and mouse input system
        Log.h/cpp         Logging system
        main.cpp          Entry point and main loop
    docs/                 Style guide and design notes
    CMakeLists.txt        Build configuration
    CMakePresets.json     Build preset definitions
    .clang-format         Code formatting rules
    Doxyfile              Doxygen configuration (API docs build from this)

The engine will get split into proper subdirectories under `src/`
(`platform/`, `render/`, `editor/`, etc.) once the structure justifies it.

## Design notes

Some non-default decisions worth knowing about:

- **No exceptions, no RTTI.** Compiled with `-fno-exceptions -fno-rtti`.
  Allocation failures and constructor failures are reported via factory
  functions returning `nullptr` or `bool` status.
- **No third-party libraries.** The engine talks directly to the OS and
  the C++ standard library and nothing else. The Vulkan SDK is the only
  exception, and only because the GPU is not addressable from userspace
  any other way.
- **Singletons for engine-wide subsystems** (Log, Input). Explicit
  Init/Shutdown lifecycle, not lazy-init.
- **PIMPL for any class whose implementation requires platform headers.**
  This keeps `windows.h` out of every translation unit that uses a
  Window.

For more detail, see `docs/STYLE.md`.

## License

All rights reserved. The code is public for visibility; no rights to use,
copy, modify, or redistribute are granted. If you have a specific use in
mind, ask.