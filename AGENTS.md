# BoringNotch

"Dynamic Island" / notch-style overlay for Windows — always-on-top glass-morphism bar at top center showing media controls, system stats, and time.

## Toolchain

- **Compiler:** LLVM-mingw clang++ (`C:\Users\AymanCassim\llvm-mingw\llvm-mingw-20260616-msvcrt-x86_64\bin\clang++.exe`)
- **Build:** CMake 4.4.0 (`C:\Program Files\CMake\bin\cmake.exe` — not in PATH, always use full path)
- **Make:** mingw32-make (same LLVM-mingw bin dir)
- **Python:** `C:\Python314\python.exe`

## Running code (critical)

Corporate AV blocks all .exe files. **The only way to run is:**

```
python runner.py
```

This compiles `OptiNotch_shared.dll` (name TBD, will rename with project) and loads it via `ctypes.CDLL`, calling the exported `run()` function.

VS Code `Ctrl+Shift+B` runs `python runner.py` (configured in `.vscode/tasks.json`).

## Build targets

| Target | Type | Entry | Purpose |
|--------|------|-------|---------|
| `OptiNotch` | Executable | `main()` | Unusable (AV blocked) |
| `OptiNotch_shared` | Shared lib (DLL) | `extern "C" run()` | Loaded by `runner.py` |

The DLL target defines `OPTINOTCH_BUILD_AS_DLL` to export symbols via `__declspec(dllexport)`.

## Build commands

```powershell
# First-time configure (or when CMakeLists.txt changes)
& "C:\Program Files\CMake\bin\cmake.exe" -B build -G "MinGW Makefiles" `
    "-DCMAKE_CXX_COMPILER=C:\Users\AymanCassim\llvm-mingw\llvm-mingw-20260616-msvcrt-x86_64\bin\clang++.exe" `
    "-DCMAKE_MAKE_PROGRAM=C:\Users\AymanCassim\llvm-mingw\llvm-mingw-20260616-msvcrt-x86_64\bin\mingw32-make.exe"

# Build only
& "C:\Program Files\CMake\bin\cmake.exe" --build build

# Build + run (the normal workflow)
python runner.py
```

## Project structure

```
boringnotch/
├── src/
│   ├── main.cpp           # Win32 entry point + message pump
│   ├── window.cpp/h       # Win32 window creation + transparent overlay
│   ├── renderer.cpp/h     # ImGui + DX11 initialization and rendering
│   ├── ui.cpp/h           # Notch UI layout
│   ├── media.cpp/h        # SMTC integration (track info + controls)
│   ├── system.cpp/h       # CPU/RAM/battery stats
│   ├── settings.cpp/h     # JSON config loading/saving
│   └── tray.cpp/h         # System tray icon + context menu
├── external/
│   ├── imgui/             # Dear ImGui (vendored, gitignored)
│   └── json/              # nlohmann/json single header
├── runner.py              # build + run via Python ctypes
└── CMakeLists.txt         # C++17
```

## Core features (by priority)

1. **Window:** Always-on-top, no taskbar entry, layered transparency, click-through by default (clickable UI on hover), top-center positioning (~300x80), no border.
2. **UI:** Glass-morphism dark background, media section (track + artist), media controls (play/pause/prev/next), volume indicator, CPU/RAM %, clock (HH:MM:SS).
3. **Media (SMTC):** Read track info from active session (Spotify/YouTube/VLC), play/pause/next/prev, subscribe to events.
4. **System stats:** CPU % (perf counters), RAM % (GlobalMemoryStatusEx), battery (if laptop).
5. **Config:** JSON at `%APPDATA%\BoringNotch\config.json`, load on startup, save on changes.
6. **System tray:** Shell_NotifyIcon with context menu (show/hide, settings, quit).
7. **Global hotkey:** `Win+Shift+N` via RegisterHotKey to toggle visibility.
8. **Performance:** 60 FPS, background threads for stat polling, event-driven SMTC.

## Key technical challenges

| Challenge | Solution |
|-----------|----------|
| Transparent window | `WS_EX_LAYERED` + alpha compositing + clear with `(0,0,0,0)` |
| Click-through + UI clicks | `WM_NCHITTEST` returning `HTTRANSPARENT` / `HTCLIENT` based on `ImGui::IsAnyItemHovered()` |
| SMTC in desktop app | WinRT `Windows.Media.Playback.MediaPlayer` as bridge, `-lwindowsapp` |
| LLVM MinGW + WinRT | Include WinRT headers, `CoInitialize`, link `windowsapp` |
| DPI scaling | `GetDpiForWindow()` + scale fonts/sizes |

## Development phases

1. **Foundation** — toolchain, ImGui + DX11 compiling, transparent always-on-top window, click-through
2. **UI Layout** — notch UI design, glass-morphism styling, placeholders
3. **Media (SMTC)** — WinRT setup, track info, playback controls, events
4. **System stats** — CPU/RAM polling, clock, battery
5. **System integration** — tray icon, hotkey, config load/save
6. **Polish** — animations, error handling, performance, cross-Windows testing

## DLL gotchas

- C++ stdlib and libgcc linked statically in the DLL target (`-static-libstdc++ -static-libgcc`) to avoid missing runtime DLLs at load time.
- The window + D3D11 device + message loop live inside the `run()` function — the loop blocks until the window closes.
- `runner.py` calls `os.chdir()` to the repo root before loading the DLL, so relative paths in C++ resolve from the project root.
- Initially `__declspec(dllexport)` via `OPTINOTCH_BUILD_AS_DLL` define — will rename macros with project.

## ImGui integration

- Cloned into `external/imgui/` (or `imgui/` — consistent with layout above).
- Backend: `imgui_impl_win32` + `imgui_impl_dx11`.
- Linked against `d3d11`, `dwmapi`, `d3dcompiler` (Windows SDK).
- The `run()` function owns the entire window lifecycle.
