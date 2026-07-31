# OptiNotch

"Dynamic Island" / notch-style overlay for Windows — always-on-top glass bar at the top-center of the screen showing a clock, expanding to a media/calendar layout on hover.

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

This builds `libOptiNotch_shared.dll` and loads it via `ctypes.CDLL`, calling the exported `run()` function. `run()` **blocks until the window closes**, so a shell running it will hang — that is normal.

**Before rebuilding, kill any running instance first** or the linker fails with `ld.lld: error: failed to write output 'libOptiNotch_shared.dll': Permission denied` (the DLL is locked while loaded):

```powershell
Get-Process | Where-Object { $_.Name -match 'python|OptiNotch' } | Stop-Process -Force
& "C:\Program Files\CMake\bin\cmake.exe" --build build
```

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
```

## Project structure (current)

```
src/
├── main.cpp           # D3D11 + DirectComposition init, message loop, run()
├── window/
│   ├── window.cpp/h   # window creation, hover/expand animation, resize + RTV rebuild
└── ui/
    ├── main_ui.cpp/h  # island silhouette + clock UI (render_ui)
imgui/                 # vendored Dear ImGui (repo root, NOT external/)
assets/fonts/          # Inter-Regular.ttf, Inter-SemiBold.ttf
runner.py              # build + run via Python ctypes
CMakeLists.txt         # C++17, links d3d11 dxgi dwmapi d3dcompiler dcomp
```

Only the window + clock UI exist so far. Media/SMTC, system stats, settings/tray, hotkey, and click-through are **not implemented** (files `renderer.cpp`, `media.cpp`, `system.cpp`, etc. in older docs don't exist).

## Transparency: DirectComposition (verified working)

This is the hard-won part. The transparent overlay uses **DirectComposition**, not a layered window:

- Window style is `WS_EX_TOPMOST | WS_EX_TOOLWINDOW` — **NOT `WS_EX_LAYERED`**. DComp handles compositing.
- `D3D11CreateDevice` must use `D3D11_CREATE_DEVICE_BGRA_SUPPORT` (per-pixel alpha requires it).
- Swapchain created via `IDXGIFactory2::CreateSwapChainForComposition` with `DXGI_ALPHA_MODE_PREMULTIPLIED`, `DXGI_SWAP_EFFECT_FLIP_DISCARD`, `DXGI_FORMAT_B8G8R8A8_UNORM`.
- Set up DComp device/target/visual: `DCompositionCreateDevice` → `CreateTargetForHwnd(hwnd, TRUE, ...)` → `CreateVisual` → `visual->SetContent(swapchain)` → `target->SetRoot(visual)`.
- **Call `g_dcomp_device->Commit()` after every `Present()`** — without it nothing shows.
- Clear color is `(0,0,0,0)`; the island fill carries the alpha.

### Do NOT do these (dead ends already tried)

- **Alpha-blended HWND swapchains are forbidden by DXGI.** `CreateSwapChainForHwnd` + `DXGI_ALPHA_MODE_PREMULTIPLIED` always returns `DXGI_ERROR_INVALID_CALL (0x887A0001)` — DXGI requires `CreateSwapChainForComposition` or CoreWindow. Do not retry flip-model premultiplied on an HWND.
- `SetWindowRgn` + `SetLayeredWindowAttributes` = aliased corners, abandon. Also removed: setting `WS_EX_LAYERED`.

## Island silhouette (src/ui/main_ui.cpp)

`draw_island_shape(dl, ox, oy, W, H, alpha)` draws the notch:

- **Top edge is a straight line flush with the screen top** (`y = oy`) across the full width — no inset, so there is no visible padding below the screen edge.
- **Bottom corners are concave** (inward-rounded, radius `rb`).
- Fill `ImVec4(0.05, 0.05, 0.07, alpha)`; stroke white at `0.07 * alpha`.

Gotchas in this ImGui build:
- `PathBezierQuadCurveTo` does **not exist** — the corner beziers are emitted as a loop of `PathLineTo` points.
- Use `fminf()` and a local `PI` constant; `ImMin`/`IM_PI` are not defined here.
- `PathFillConcave` and `PathStroke` do exist.

## Window behavior (src/window/window.cpp)

- Constants: `NOTCH_WIDTH=120`, `NOTCH_HEIGHT=20`, `EXPANDED_WIDTH=600`, `EXPANDED_HEIGHT=150`.
- Window is **fixed** at `EXPANDED_WIDTH x EXPANDED_HEIGHT`, top-center, **`y=0`** (flush with screen top). It is created and swapped at the expanded size; **no per-frame `SetWindowPos`/`ResizeBuffers`** — the swapchain/RTV are created once in `create_d3d11()`.
- The island grows/shrinks symmetrically inside the fixed window: `render_ui()` computes `iw`/`ih` from `g_progress` (`120→600` x `20→150`) and offsets `ix = (width-iw)/2`, so both edges expand from center. `update_window_animation()` only updates `g_progress` and `g_window_alpha`.
- Hover expands: `g_progress += 0.09f` per frame when hovered (no Ctrl), `-= 0.09f` otherwise (clamped 0..1).
- Alpha: `g_window_alpha` = `180/255` when hovered **and** Ctrl held, else `240/255`.
- Fonts loaded in `run()`: Inter-Regular 11/14px, Inter-SemiBold 32/14px (collapsed clock uses 14px Semibold).
- `g_swap_chain` is declared `IDXGISwapChain*`; `CreateSwapChainForComposition` returns `IDXGISwapChain1*` — assign through a local variable, not `&g_swap_chain`.
- `run()` returns 1 if window creation fails, 2 if D3D11/DComp init fails, 3 if device/swapchain/RTV missing, 0 on clean close.

## ImGui integration

- Vendored at `imgui/` (repo root). Backends: `imgui_impl_win32` + `imgui_impl_dx11`.
- Linked against `d3d11`, `dxgi`, `dwmapi`, `d3dcompiler`, `dcomp`.
- `runner.py` `os.chdir()`s to repo root before loading, so relative paths like `assets/fonts/` resolve from the project root.

## DLL gotchas

- C++ stdlib and libgcc linked statically in the DLL target (`-static-libstdc++ -static-libgcc`) to avoid missing runtime DLLs at load time.
- The window + D3D11 device + message loop all live inside `run()`; the loop blocks until the window closes.
- Cleaning `build/` may be needed after CMake schema changes to avoid duplicate-symbol linker errors.
