## 2026-05-12 — Win32 A/W macro system
The Win32 API has parallel A (ANSI) and W (wide/UTF-16) variants of every
function that touches strings. The bare names (e.g. GetMessage) are macros
that expand to GetMessageA or GetMessageW depending on whether UNICODE
is defined. Mixing them produces partial failures — e.g., my window title
showed as a single character because GetMessage expanded to GetMessageA
but the window was registered with RegisterClassExW.

Resolution: defined UNICODE and _UNICODE in CMakeLists.txt via
target_compile_definitions. Also added explicit W suffix to all Win32
calls in source for clarity. (I think)

## 2026-05-12 — clangd not picking up CMake defines
After adding UNICODE to CMake, the build worked but clangd still showed
red squiggles on LoadCursorW. compile_commands.json had the defines but
clangd was somehow processing windows.h in A-mode anyway.

Resolution: created .clangd file at project root with explicit CompileFlags
Add: directive duplicating the defines. This forces clangd to apply them
regardless of how it interprets compile_commands.json.

Note: any time clangd shows different errors than the actual build,
check the .clangd file first.

## 2026-05-12 - Disabling exceptions to improve performance
1. Predictable performance. Exceptions cost essentially nothing when not thrown, but throwing one costs thousands of cycles (stack unwinding, RTTI lookups, destructor calls). Game engines have hard 16.67ms-per-frame budgets at 60fps; an exception thrown at the wrong moment can blow the frame budget. With status codes, error handling is uniform — same cost in success and failure paths.
2. Binary size. Exception machinery (unwind tables, RTTI for every type) adds significant size to the binary. For shipped games this matters.
3. Control flow visibility. With exceptions, any function call might secretly unwind your stack. With status codes, error paths are visible in source. You can read a function and see every place it can fail. This makes engine code easier to reason about, especially for systems that own GPU resources, file handles, or threads.
4. Interop with C APIs. Vulkan is a C API. It can't throw across its boundary. Mixing throwing C++ with non-throwing C means careful exception-safety analysis at every boundary. Going no-exceptions everywhere makes Vulkan integration uniform.
5. Convention with the field. Unreal, id Tech, Source, CryEngine, Frostbite, Godot — all major engines either disable exceptions entirely or restrict them severely.

## 2026-05-12 — Refactored window code into a Window class

Moved all the Win32 stuff out of main.cpp and into Window.h / Window.cpp.
main.cpp doesn't know Win32 exists anymore — it just creates a Window,
loops until ShouldClose() is true, polls events each iteration, and
deletes the window on exit. Way cleaner.

### Design decisions made

- **No exceptions, no RTTI.** Disabled both globally via -fno-exceptions
  and -fno-rtti in CMakeLists.txt. This is the game engine convention
  (Unreal, id Tech, etc. all do this). Means I have to use status codes
  and nullptr returns instead of throwing, but performance is more
  predictable and the code is more honest about where it can fail.

- **Factory function instead of public constructor.** Window::Create() is
  static and returns Window* (or nullptr on failure). The constructor is
  private. Reason: without exceptions, a constructor can't signal failure,
  so the factory pattern is cleaner than two-phase init.

- **PIMPL pattern for Win32 hiding.** Window.h forward-declares struct
  WindowImpl. The actual definition (with HWND etc.) lives in Window.cpp.
  This keeps windows.h out of every file that just wants to use a Window.

- **Window is non-copyable, non-movable.** It owns OS resources; copying
  or moving would create ownership ambiguity. Marked all four with
  = delete.

- **PeekMessage instead of GetMessage.** Old code used GetMessage which
  blocks until a message arrives — fine for a desktop app, wrong for a
  game. PeekMessage is non-blocking, so the main loop can run continuously.

### Win32 weirdness encountered

- **WM_NCCREATE bootstrap.** Need to stash WindowImpl* in GWLP_USERDATA so
  WndProc can find it, but the catch is that WndProc starts receiving
  messages during CreateWindowExW, before it returns. The standard trick
  is: pass the pointer as the last parameter to CreateWindowExW, it arrives
  in WM_NCCREATE's CREATESTRUCT, stash it from there. Every C++ wrapper
  around Win32 does this exact dance.

- **Title conversion is hacky right now.** Just byte-casting UTF-8 to
  wchar_t. Works for ASCII, wrong for anything else. TODO: write a real
  UTF-8 to UTF-16 converter using MultiByteToWideChar when I actually
  need non-ASCII window titles.

### Files at end of this session

- Common.h — engine-wide types (u8/u16/u32/etc., Status enum)
- Window.h — Window class interface
- Window.cpp — Win32 implementation
- main.cpp — minimal, uses Window class
- CMakeLists.txt — has -fno-exceptions, -fno-rtti, UNICODE defines
- .clangd — explicit compile flags so the editor matches the build
- CMakePresets.json — windows-debug and windows-release configs

### Next steps

- Refactor main.cpp's `delete window` into std::unique_ptr for proper RAII
- Maybe: start initializing Vulkan