# Development Journal
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

## 2026-05-13 — Keyboard and mouse input system

Built a singleton-based input system. apex::Input::Init() / Shutdown() /
Get() lifecycle, with platform-independent Key and MouseButton enums.
Window.cpp translates Win32 virtual key codes into engine Key values and
feeds events to the singleton.

### Design decisions

- **Singleton (Architecture C).** Init/Shutdown explicit lifecycle, not the
  Meyers lazy-init pattern. Reasoning: future subsystems (renderer, audio)
  will have init order dependencies, so explicit ordering matters. Better
  to use one consistent pattern for all engine subsystems.

- **Polled input, not event-driven.** Game code asks "is W down?" rather
  than subscribing to callbacks. Standard for games — fits the per-frame
  loop and keeps logic together instead of scattered across handlers.

- **Two-array state pattern.** m_keysDown (current) + m_keysDownLastFrame
  (snapshot). NewFrame() copies current into last. IsKeyDown reads current;
  WasKeyPressed/WasKeyReleased compare them. Cheap and elegant.

- **"Is" vs "Was" naming convention.** Is = current state, Was = edge this
  frame. Adopt consistently across the engine.

- **Count sentinel in enums.** Last value of Key and MouseButton enums is
  Count, used to size state arrays. Adding a key auto-updates array sizes
  everywhere.

### Win32 details worth remembering

- **Auto-repeat filtering.** WM_KEYDOWN fires repeatedly while a key is
  held. Bit 30 of lParam is 1 if the key was already down before this
  message (i.e. it's a repeat). Check (lParam & (1 << 30)) and skip if set.

- **WM_KEYDOWN vs WM_SYSKEYDOWN.** SYSKEYDOWN fires when Alt is held. Treat
  identically for our purposes.

- **SetCapture / ReleaseCapture for mouse buttons.** Without capture, mouse
  events stop when the cursor leaves the window. Capture on button-down,
  release on button-up.

- **GET_X_LPARAM / GET_Y_LPARAM, not LOWORD/HIWORD, for mouse coords.**
  These handle sign extension correctly — coords can be negative when the
  mouse is captured and moves off the window's top/left edge. Lives in
  windowsx.h, separate from windows.h.

- **Mouse wheel delta is in HIWORD(wParam), signed (cast to i16).**
  Standard notch is ±120 units; divide by 120 to get clean ±1 per notch.

- **WM_KILLFOCUS handler.** Clear all input state when window loses focus,
  so keys held during an alt-tab don't appear stuck when focus returns.

- **VK_LMENU / VK_RMENU = Alt keys.** "Menu" is Microsoft's old name for
  Alt. Confusing.

### Bug hit and lesson learned

In WM_KEYUP handler I called OnKeyDown instead of OnKeyUp — classic
copy-paste from the WM_KEYDOWN block where I forgot to change the method
name. Symptom: "Space pressed" fires but "Space released" never does,
because the input system thinks the key is still held. WasKeyPressed also
stops firing on subsequent presses because the auto-repeat filter sees
the (stuck) down state and skips it.

**Lesson: when two case blocks look almost identical, the bug is almost
certainly in the one tiny part that differs.** Code that's symmetric
visually is exactly where you forget to flip one detail. Worth scanning
for this pattern any time you copy-paste a case block.

**Tool lesson: a breakpoint in OnKeyUp would have proven instantly that
the function was never called.** Reach for the debugger when behavior
contradicts what the code "should" do — it's faster than re-reading the
source for the third time.

### What's NOT in the input system yet (deferred)

- Gamepad support
- Text input (WM_CHAR, IME)
- Raw input mode for FPS cameras (DirectInput / RawInput API)
- Input contexts / consume mechanism (for routing input between editor
  and game eventually)
- Re-syncing state on WM_SETFOCUS by querying currently-held keys
- Asserting in Input::Get() that Init() was called (waiting on logging
  system to give us a real assert macro)
- Real UTF-8 to UTF-16 conversion for window titles (still byte-casting)

### Files at end of this session

- Input.h, Input.cpp — new
- Window.cpp — added keyboard/mouse handling + virtual key translation
- main.cpp — wired up Init/Shutdown/NewFrame and test prints
- CMakeLists.txt — added Input.cpp to source list

### Next steps

- Logging system (printf is fine but real engine needs levels, timestamps,
  file output)
- After logging: add asserts to Input::Get() and other invariant checks
- Then: Vulkan

## 2026-05-13 (later) — Logging system

Built a singleton-based logging system with severity levels, per-category
thresholds, console + file + OutputDebugString sinks, ANSI colors on the
console, and timestamped log files in logs/.

### Design decisions

- **Singleton with explicit Init/Shutdown lifecycle** (consistent with Input).
- **All six severity levels** (Trace, Debug, Info, Warn, Error, Fatal) plus
  Off as a threshold sentinel.
- **Per-category levels**, defaulting to global default. Stored in
  unordered_map<string, LogLevel>.
- **std::format for the format string syntax.** Compile-time-checked when
  using literal format strings (which is 100% of our case). NOT
  printf-style — uses {} placeholders.
- **State in file-scope (anonymous namespace) instead of Log members.**
  Keeps <mutex>, <fstream>, <unordered_map> out of Log.h, reducing
  compile-time dependencies for every file that includes Log.h.
- **Log files: logs/apex_YYYY-MM-DD_HH-MM-SS.log, one per run.**
- **LOG_FATAL calls std::abort() after logging.** Unrecoverable error =
  don't trust the program to clean up gracefully.
- **Macros wrap the API** so __FILE__ and __LINE__ are captured at the
  call site, and so the IsEnabled() check runs before any std::format
  work.

### Win32 details

- **EnableConsoleColors() via ENABLE_VIRTUAL_TERMINAL_PROCESSING.**
  Without this, ANSI codes print literally instead of being interpreted.
  Set once at startup on the stdout handle.
- **localtime_s for thread-safe local time conversion on Windows.**
  Regular localtime returns a static buffer and isn't thread-safe.
- **OutputDebugStringA so log messages appear in the debugger's output
  window** when running under F5 in VS Code.

### Bugs hit and lessons

**Bug 1: std::format silently accepts wrong format specifiers.**
I wrote `LOG_DEBUG("Input", "...at (%d, %d)...", x, y)` — printf-style.
std::format is fine with that string (it has zero {} placeholders) and
silently ignores the two extra arguments. Output had literal "%d, %d" in
it. Compiler did NOT warn. Lesson: must mentally switch from printf to
std::format syntax. Brace-style: `({}, {})`.

**Bug 2: Subsystem shutdown order matters.**
Window's WndProc calls Input::Get() during message dispatch. When
DestroyWindow fires WM_KILLFOCUS (which it does during teardown), my old
shutdown order had already called Input::Shutdown() — so Input::Get()
dereferenced a null pointer and the program died silently between two
log calls. Fixed by reversing init order: Input before Window in init,
so that shutdown order Window-then-Input has dependencies correctly
unwound.

**Lesson: shutdown order isn't always the reverse of init order — it
depends on the dependency graph.** Window depends on Input → Window
must be destroyed first → Input must be initialized first. The general
rule: subsystems with dependents go down last.

**Lesson: silent program death between log calls is almost always
"something in between crashed."** Use printf-debugging (in this case
log-debugging) by adding messages around each suspect call to bisect
which one is the killer.

### Files at end of session

- Log.h, Log.cpp — new
- main.cpp — wired up Log, reordered Input/Window init
- Window.cpp — replaced printf with LOG_ERROR
- CMakeLists.txt — added Log.cpp
- .gitignore — added logs/

### Next steps

- Code cleanup pass with clang-format
- Doxygen setup + documentation pass on public API
- GitHub Pages hosting
- Then: Vulkan