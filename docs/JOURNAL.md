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
