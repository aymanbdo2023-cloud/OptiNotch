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

# Release package (separate build-release/ dir): python tools/make_release.py
```

The release zip lands in `dist/OptiNotch-<ver>.zip` containing `OptiNotch.exe` (statically linked, no runtime DLLs, fonts embedded via `#embed` — fully self-contained, no `assets/` folder at runtime), `README.txt`, `gcal_credentials.example.json`, and — when found — the real **`gcal_credentials.json`** bundled from the owner's repo-root copy (or `%APPDATA%\OptiNotch\`), so end users sign in with their own Google account and never touch the Cloud console. Because fonts are embedded, the exe runs regardless of CWD — the tray "Start with Windows" just points straight at the exe.

## Project structure (current)

```
src/
├── main.cpp           # D3D11 + DirectComposition init, message loop, run()
├── window/
│   ├── window.cpp/h   # window creation, DPI scale, monitor positioning, hover/expand + hide animation
├── ui/
│   ├── main_ui.cpp/h  # island silhouette + clock + calendar/media halves (render_ui)
│   ├── settings_ui.cpp/h  # in-notch settings panel (gear icon)
│   ├── setup_ui.cpp/h     # Google Calendar first-run wizard widgets
├── settings/
│   ├── settings.cpp/h # AppSettings (settings.json), autostart registry
├── tray/
│   ├── tray.cpp/h     # system tray icon + context menu
├── calendar/
│   ├── calendar.cpp/h # Google Calendar OAuth2 + events
│   ├── http.cpp/h     # WinHTTP wrapper
│   ├── json.cpp/h     # minimal JSON parser
├── media/
│   ├── media.cpp/h    # Windows.Media.Control (SMTC) session manager, album art
imgui/                 # vendored Dear ImGui (repo root, NOT external/)
assets/fonts/          # Inter-Regular.ttf, Inter-SemiBold.ttf
runner.py              # dev build + run via Python ctypes
tools/
    ├── make_release.py    # Release build -> dist/OptiNotch-<ver>.zip
    ├── README.txt         # shipped in the release zip
    └── gcal_credentials.example.json
CMakeLists.txt         # C++17; libs: d3d11 dxgi dwmapi d3dcompiler dcomp runtimeobject windowscodecs coremessaging ole32 winhttp ws2_32 shell32 gdi32 comdlg32 advapi32
```

Window + clock + media/SMTC + calendar + settings + tray + DPI are implemented. System stats and a native settings *window* are not; settings live in an in-notch panel. `OptiNotch.exe` is a self-contained release (static libstdc++/libgcc).

## Media integration (Windows.Media.Control) — verified working

`src/media/media.cpp` runs a background thread (`media_thread_main`) that polls the global session every 500ms. Debug logging is behind the `OPTINOTCH_MEDIA_DEBUG` env var (file `media_dbg.log`).

### The hard-won part: waiting on async operations

**Do NOT poll `IAsyncInfo::get_Status` and do NOT rely on `put_Completed`.** On this machine the `RequestAsync()`/`TryGetMediaPropertiesAsync()`/`OpenReadAsync()`/`ReadAsync()` operations **stay `Status==Started (1)` forever** — the status field never advances to `Completed`, and `put_Completed` returns `CO_E_NOT_SUPPORTED` then `E_ASYNC_OPERATION_NOT_STARTED (0x80000018)`. Yet `GetResults()` **succeeds and returns the real object** even while status is still `Started`.

- `wait_for_completion(op, get_results_slot, &result, timeout_ms)` **polls `GetResults()` directly** (via the raw vtable slot) until it returns a non-null object, checking status only to detect Error/Canceled. GetResults slots: `IAsyncOperation` = 8, `IAsyncOperationWithProgress` = 10.
- The `RequestAsync` op resolves in ~30ms via GetResults; session ops in 0–600ms.

### MinGW WIDL async vtable layout (critical)

MinGW's `IAsyncOperation<T>` and `IAsyncOperationWithProgress<P,R>` **do NOT fold `IAsyncInfo` into the vtable** — even though the IDL says they derive from it. The primary vtable is:

- `IAsyncOperation<T>`: `[IUnknown 0-2][IInspectable 3-5][put_Completed 6][get_Completed 7][GetResults 8]`
- `IAsyncOperationWithProgress<P,R>`: `[...][put_Progress 6][get_Progress 7][put_Completed 8][get_Completed 9][GetResults 10]`

To read status/error, **QI the op for `IAsyncInfo` (IID `00000036-0000-0000-c000-000000000046`)** — it is a separate interface with `get_Id 6, get_Status 7, get_ErrorCode 8, Cancel 9, Close 10`. Never cast an op to a struct that inherits `IAsyncInfo` — you'll call `put_Completed`/`get_Completed` as `get_Status` and crash.

### Other gotchas (this environment)

- The `RequestAsync`/session ops **only progress while the thread pumps messages** (`PeekMessage`+`DispatchMessage` in the wait loop). A `CreateDispatcherQueueController` (`DQTYPE_THREAD_CURRENT`, `DQTAT_COM_STA`, from `CoreMessaging.dll` via `coremessaging` lib) on the media thread is required to get the op past `Created`; declare it manually (do **not** `#include <dispatcherqueue.h>` — it drags in `windows.system.h` and breaks the build with `IReference<BYTE>` redefinitions).
- Do **not** use `CoWaitForMultipleHandles` in the wait loop — it can block indefinitely on the media thread. Sleep-based polling + PeekMessage pump reaches timeout reliably.
- Album art: `props->Thumbnail()` → `OpenReadAsync` → QI `IInputStream` (`905a0fe2-...`) → `ReadAsync` loop into `Windows.Storage.Streams.Buffer` (created via `IBufferFactory` `71af914d-...`, raw bytes via `IBufferByteAccess` `905a0fef-...`). ~22KB JPEG for a YouTube video.
- MinGW WIC quirk: `IWICImagingFactory` has **no `CreateDecoderFromMemory`** — use `CreateStream` → `InitializeFromMemory` → `CreateDecoderFromStream`.

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

- Constants: `NOTCH_WIDTH=120`, `NOTCH_HEIGHT=20`, `EXPANDED_WIDTH=600`, `EXPANDED_HEIGHT=190`. These are **logical px**; the physical window/swapchain are scaled by `g_ui_scale` (resolution-derived, fixed at startup; see "Sizing / scaling" below).
- Window is **fixed** at `EXPANDED_WIDTH x EXPANDED_HEIGHT` (scaled), top-center of the configured monitor's **work area**, flush with its top. Created and swapped at the expanded size; **no per-frame `SetWindowPos`/`ResizeBuffers`** — the swapchain/RTV are created once in `create_d3d11()`.
- The island grows/shrinks symmetrically inside the fixed window: `render_ui()` computes `iw`/`ih` from `g_progress` (`120→600` x `20→190`) and offsets `ix = (width-iw)/2`, so both edges expand from center. `update_window_animation()` only updates `g_progress` and `g_window_alpha`.
- **Expanded layout = header bar + content section.** `render_ui()` reserves a 24px header (`HEADER_H`) at the top of the expanded island: the clock (left, `ix+18`) and the settings gear (right, `ix+iw-22`, own `##gear_hit` child so it stays top-most) vertically centered. The `island_content` child starts at `iy + HEADER_H` with height `ih - HEADER_H`, so the calendar/media/settings halves keep their exact internal layout at the same ~166px content height — they are just shifted down. The calendar/media vertical divider spans `iy + HEADER_H + 12` → `iy + ih - 12`. The calendar half's own top-row clock was removed when the header clock arrived; the media half's vertical stack is biased ~8px upward (`cy0 = o.y + (H-102)*0.5 - 8`).
- Hover expands: `g_progress += 0.12f` per frame when hovered, `-= 0.12f` otherwise (clamped 0..1). `point_over_island()`/`update_window_region()` use `island_window_rect()` in **physical** px (logical constants × `g_ui_scale`). While the settings panel is open (`ui_settings_open()`), the notch is treated as hovered AND hide-override is cancelled, so opening Settings from the tray always reveals the expanded notch.
- **Hide**: animated via `g_hide` 0→1. Want-hidden = (Win+Alt held AND `settings.hide_hotkey`) OR (`g_hide_override` AND NOT settings open) (tray "Show/Hide"). Window slides to `y = g_base_y - phys_h*g_hide`, opacity via raw vtable slot 25.
- **Quit**: tray menu "Quit" or the global hotkey **Ctrl+Alt+Q** (`RegisterHotKey` → `WM_HOTKEY` → `WM_DESTROY`, registered in `create_window()`). Works while hidden/unfocused; auto-released on process exit.
- **Opacity** comes from `settings.json`: `g_window_alpha = opacity_normal/255` (default 240).
- **Positioning**: `window_apply_position()` reads `settings.monitor_index` (-1=primary, else EnumDisplayMonitors index) + `settings.x_offset` and centers the window; re-called whenever settings change. `monitor_work_area()` callback walks monitors.
- Fonts loaded in `run()` at `11/14/32/14 × g_ui_scale` px; `io.FontGlobalScale = 1/scale` keeps logical sizes.
- `g_swap_chain` is declared `IDXGISwapChain*`; `CreateSwapChainForComposition` returns `IDXGISwapChain1*` — assign through a local variable, not `&g_swap_chain`.
- `run()` returns 1 if window creation fails, 2 if D3D11/DComp init fails, 3 if device/swapchain/RTV missing, 0 on clean close.

## ImGui integration

- Vendored at `imgui/` (repo root). Backends: `imgui_impl_win32` + `imgui_impl_dx11`. `imgui_impl_win32` carries a small OptiNotch patch (`ImGui_ImplWin32_SetMouseScale`) so the mouse is reported in logical px; keep it if the backend is ever updated.
- UI fonts (Inter Regular/SemiBold) are embedded in `src/main.cpp` via `#embed` (clang extension/C23) and loaded with `AddFontFromMemoryTTF` + `FontDataOwnedByAtlas=false`, so the app is self-contained. The system icon font still loads from `C:/Windows/Fonts/SegMDL2.ttf`. This replaced an earlier RCDATA-resource approach that failed at runtime (lld resource-data bug, GetLastError 1812).

## Sizing / scaling (framebuffer-scale trick)

The whole UI is laid out in **logical px** (600×190) and scaled up by `g_ui_scale`:

- `ImGui_ImplWin32_EnableDpiAwareness()` at the top of `run()`, before any window (keeps monitor geometry in physical px).
- `g_ui_scale` = `NOTCH_WIDTH_FRACTION * screen_width / NOTCH_WIDTH`, where `screen_width` is the chosen monitor's **physical width** — so the collapsed island is a fixed 8% of the screen width and everything scales with the **resolution**, not the DPI. Clamped ≥ `MIN_UI_SCALE` (0.7), computed in `create_window()`, **fixed at startup**. Tune the size with `NOTCH_WIDTH_FRACTION` in `window.h`.
- Window + swapchain are created at physical size (`600×scale × 190×scale`).
- Each frame after `ImGui_ImplWin32_NewFrame()`, `main.cpp` overrides `io.DisplaySize = (600,190)` (logical) and `io.DisplayFramebufferScale = (scale,scale)`. The vendored DX11 backend honors `draw_data->FramebufferScale`, so all geometry scales automatically.
- Fonts loaded at `size × scale` so glyphs rasterize crisp; `io.FontGlobalScale = 1/scale` keeps `ImGui::GetFontSize()` in logical px for layout.
- The Win32 backend reports the mouse in **physical** client px; `ImGui_ImplWin32_SetMouseScale(g_ui_scale)` (called in `run()` after `create_window`) makes it report **logical** px instead, matching `io.DisplaySize`. This must happen in the backend (not by dividing `io.MousePos` after `NewFrame()`), because ImGui's `UpdateHoveredWindowAndCaptureFlags()` runs inside `NewFrame()` with the reported position — dividing late left it physical against logical window rects and broke all hover/click at scale ≠ 1.
- On a 1366×768 screen at 100% DPI the scale is ≈0.91; DPI scaling itself is otherwise ignored for sizing.

## DLL gotchas

- C++ stdlib and libgcc linked statically in **both** targets (`-static-libstdc++ -static-libgcc`) so `OptiNotch.exe` ships with no runtime DLLs.
- The window + D3D11 device + message loop all live inside `run()`; the loop blocks until the window closes.
- Cleaning `build/` may be needed after CMake schema changes to avoid duplicate-symbol linker errors.

## Settings (src/settings/) — `%APPDATA%\OptiNotch\settings.json`

`AppSettings` holds: `monitor_index` (-1=primary), `x_offset`, `hide_hotkey`, `start_with_windows`, `accent_r/g/b`, `opacity_normal` (0..255), `media_enabled`, `calendar_enabled`. Access via `settings_get()`/`settings_set()` (mutex-guarded); `settings_save()` writes JSON; `settings_apply_autostart()` syncs the `HKCU\...\Run` "OptiNotch" value (points to the exe, or to `python runner.py` when running under python). UI reads the accent/opacity every frame via `accent_u32()` in `main_ui.cpp`.

## Tray (src/tray/)

`tray_init()` registers a `Shell_NotifyIcon` that posts `TRAY_CALLBACK` (`WM_APP+1`) to the notch window; `window.cpp`'s wnd_proc forwards to `tray_handle_message()`. Menu: Show/Hide (`window_set_hidden`), Settings (`ui_open_settings_panel`), Start with Windows + Win+Alt toggles (settings, `MF_CHECKED`), Quit (`PostMessage(WM_DESTROY)`). Icon is a 32×32 monochrome island drawn programmatically (AND=1 transparent, XOR=1 black).

## Settings UI (src/ui/settings_ui.cpp) + gear

Opened from the tray or the gear glyph (Segoe MDL2 `E713`) at the island's top-right; replaces the left/right halves in the expanded notch. Rows: monitor combo (`EnumDisplayMonitors`), position offset slider, Win+Alt + autostart checkboxes, accent `ColorEdit3`, two opacity sliders, a Google Calendar account section ("Sign out" when authed / "Connect Google Calendar" when not), Done. Changes call `settings_set` + `settings_save` + `window_apply_position`. The position-offset slider persists live but **only repositions the window on release** (`g_xoff_dragging` edge detection) — repositioning while dragging slides the slider under the cursor and races the notch to the screen edge. Scrollable child like the events area.

## Calendar (Google Calendar API) — src/calendar/

`calendar.h/.cpp` + `json.h/.cpp` (minimal JSON parser) + `http.h/.cpp` (WinHTTP wrapper). Links `winhttp` + `ws2_32`. A background thread (`cal_thread_main`) polls a `refresh_requested` flag every 300ms; fetches events for the displayed month and caches in `g_cal` (guarded by `g_cal_mutex`). UI reads `g_cal` under the same mutex; never call blocking/network cal functions from the UI thread.

### OAuth2 flow (verified working)

- Credentials file **`gcal_credentials.json`** — resolved, in order, from **`%APPDATA%\OptiNotch\`** (per-user, written by the wizard), the **exe's own directory** (bundled creds shipped with the release), then **repo root** (dev CWD fallback). Users get them via the setup wizard (`src/ui/setup_ui.cpp`): "Google Cloud setup" opens the console, "Load client JSON…" opens a file picker and `cal_import_credentials_file()` copies the chosen OAuth Desktop client JSON to APPDATA. When bundled creds exist the wizard is skipped entirely — the calendar half shows "Connect Google Calendar" straight away. Accepted formats: the flat OptiNotch form below **and** Google's raw `installed`/`web` wrapper:
  ```json
  { "client_id": "...apps.googleusercontent.com", "client_secret": "...",
    "calendar_id": "primary", "redirect_port": 8080 }
  ```
- `cal_thread_main` reloads credentials when the user imports them mid-run (the `!creds.ok` branch re-runs `load_credentials()` every 500ms).
- Scope `https://www.googleapis.com/auth/calendar.readonly`, `access_type=offline`, `prompt=consent` (forces a fresh refresh_token).
- Auth: open browser to the consent URL, then **listen on `http://127.0.0.1:<redirect_port>/` with a `socket`** (blocking listen+accept on the auth thread; server closes after the first response). The one-shot code is swapped for tokens (refresh_token granted once, then stored).
- Tokens stored at `%APPDATA%\OptiNotch\gcal_token.json` (`access_token`, `refresh_token`, `expires_at` epoch). No token file → starts unauthenticated; UI shows a "Connect Google Calendar" button that calls `cal_start_auth()`.
- **Sign out** (`cal_sign_out()`): deletes `gcal_token.json`, sets `g_cal.sign_out_requested` (processed by the thread → drops in-memory token/events) and `auth_attempted=true` so the browser does **not** auto-reopen after sign-out. The user reconnects via the Connect button; `prompt=consent` shows Google's account picker, so signing in again with a different account switches calendars. A "Sign out"/"Connect" section lives in the settings panel (`settings_ui.cpp`).
- If the events fetch returns 401, `refresh_access()` uses the refresh_token, saves, and retries once.
- Calendar view state: `g_cal.year/month` (month 0-11), `today_day`, `selected_day`, `status`. `cal_goto_month(±1)` and `cal_select_day(day)` set flags the background thread picks up.
- **MinGW gotcha:** `URL_COMPONENTSW` scheme member is `nScheme` (INT enum), **not** `dwScheme` — `dwScheme` doesn't exist in MinGW's struct. And MinGW does not ship an `INTERNET_SCHEME_HTTPS` mismatch issue — it's `nScheme == INTERNET_SCHEME_HTTPS`.
- CMakeLists: calendar sources added to both targets; link list now includes `winhttp ws2_32`.

## Calendar UI (src/ui/main_ui.cpp, render_left_half)

Left half of the expanded notch, currently 45% of width. Layout top→bottom inside `W=270, H=150`:
- Month header ("Aug 2026", 14px Semibold) centered at `y≈10`.
- Nav arrows (Segoe MDL2 E9AB prev / E9AC next, `draw_icon_button`) at `y≈19`, calling `cal_goto_month(±1)`.
- Divider at `y≈34`; weekday header row ("Sun"…"Sat") at `y≈41`; **single week row** of dates at `y≈55` (18px cells). Dates are aligned by `cal_day_of_week(year,month,1)` — days before/after the month render as empty cells. Today = filled white circle with dark text; selected = white ring; hover = faint circle. Clicking calls `cal_select_day(day)`.
- Divider at `y≈78`; events area from `y≈86`: up to 3 events, green dot + ellipsized title (60% width) + right-aligned time metadata; `+N more` when overflow, else `status` line (e.g. "Calendar sync failed").
- States: no credentials → setup wizard (`render_cal_setup_widgets`: "Google Cloud setup" + "Load client JSON…"); unauthenticated → "Connect Google Calendar" pill button (`draw_accent_button`, triggers `cal_start_auth()`) + status text; syncing → "Loading…"; else events.
- The date row is single-week (Mon–Sun of the displayed month's first week). This is a deliberate minimal choice — revisit if a multi-week grid is wanted.
