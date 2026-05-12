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

Tag this as: any time clangd shows different errors than the actual build,
check the .clangd file first.