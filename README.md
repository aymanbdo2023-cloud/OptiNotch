# OptiNotch

A "Dynamic Island" style overlay for Windows — an always-on-top glass bar at the
top-center of your screen showing a live clock. Hover it and it expands into
your **Google Calendar** and the **currently-playing media** (Windows
Media.SMTC), plus an in-notch **settings panel**.

Think of it as an iPhone-style Dynamic Island / notch for Windows: minimal when
idle, informative when you need it, and gone when you don't.

---

## Features

- **Live clock bar** — a compact notch (120×20 logical px) pinned to the
  top-center of your chosen monitor that shows the current time.
- **Hover to expand** — the island grows symmetrically into a 600×190 overlay
  with two content halves:
  - **Calendar** — the current month (single-week view), today highlighted with
    the accent color, event dots, and up-to-3 upcoming events for the day with
    times.
  - **Media** — album art, title/artist/album, playback status, prev/play/pause/
    next controls, and a scrubbing progress bar with elapsed/total time.
- **Header bar** — a slim strip at the top of the expanded island with the
  clock (left) and a gear button (right) that opens the settings panel.
- **Google Calendar integration** — OAuth2 sign-in with your own Google account,
  read-only events, month navigation, "go to today", and an account
  **Sign out** to switch to a different Google account.
- **Windows Media integration** — uses the Windows.Media.Control (SMTC) global
  session to show what's playing anywhere on the system.
- **System tray** — icon + menu: show/hide the notch, open Settings, toggle
  **Start with Windows** and the **Win+Alt** hide hotkey, or Quit.
- **Win+Alt hotkey** — hold to slide the notch off the top of the screen.
- **Ctrl+Alt+Q** — global quit hotkey, closes the notch from anywhere (even when hidden).
- **Settings panel** (in-notch, gear icon or tray) — monitor selection,
  horizontal position offset, hide hotkey, start-with-Windows, accent color,
  opacity, and Google account management.
- **Multi-monitor & resolution-sized** — the notch tracks the work area of your
  chosen monitor and scales with the screen's resolution (a fixed % of the
  screen width), so it looks proportional on any display regardless of DPI.
- **Start with Windows** — registers a `HKCU\...\Run` entry pointing at the exe
  (fonts are embedded, so it runs fine regardless of the working directory).

---

## What it's for

OptiNotch is for anyone who wants glanceable calendar and media info without a
big always-open widget, panel, or taskbar clutter:

- See the **next event / today's agenda** by just hovering the top of your screen.
- See **what's playing and control it** (skip, pause, play) without switching apps.
- Works as a **replacement for a static clock widget** with a modern, minimal
  aesthetic.
- Runs quietly in the **system tray**, starts with Windows, and stays out of the
  way until you need it.

---

## Requirements

- **Windows 10 / 11** (uses DirectComposition, Windows.Media.Control / SMTC,
  and Windows Imaging Component).
- For end users: just the release ZIP — no runtime dependencies to install.
- For developers: LLVM-mingw clang++, CMake, and Python (see *Development*).

---

## Installation (end users)

1. Download `OptiNotch-<ver>.zip` from `dist/` (or a release).
2. Unzip anywhere; the exe is fully self-contained (fonts are embedded), so
   there is no separate assets/ folder to keep together.
3. Double-click `OptiNotch.exe`. It runs from the tray.
4. Optional: tray menu → **Start with Windows** to launch at login.
5. Optional: hover the expanded notch → gear → **Settings** to pick your
   monitor, reposition it, change the accent color/opacity, etc.

> Antivirus note: some corporate AVs block unsigned `.exe` files. If the bar
> never appears, add an exception for `OptiNotch.exe`.

### Google Calendar

The app ships with **built-in Google credentials**, so you only need a Google
account:

1. Hover the notch to expand it.
2. On the calendar side, click **Connect Google Calendar**.
3. Sign in with the account you want and approve read-only access.

To **switch accounts**: gear → Settings → **Sign out**, then press
**Connect Google Calendar** in the notch again and pick another account.

(The first-run setup wizard with "Google Cloud setup" / "Load client JSON…"
only appears if the app was built *without* bundled credentials.)

---

## Usage

| Action | Result |
|--------|--------|
| Hover the notch | Expands into calendar + media |
| Move the mouse away | Collapses back to the clock |
| Gear (top-right, expanded) or tray → Settings | Opens the settings panel |
| **Win+Alt** (hold) | Slides the notch off-screen |
| Tray → Show / Hide notch | Shows / hides it |
| Calendar: arrows | Previous / next month |
| Calendar: click the month title | Jump back to today |
| Calendar: click a day | Show that day's events |
| Calendar: refresh icon | Force a calendar sync |
| Media controls | Skip, play/pause, next |

Settings are saved to `%APPDATA%\OptiNotch\settings.json`.

---

## Development

### Toolchain

- **Compiler:** LLVM-mingw `clang++`
- **Build system:** CMake 4.4+ with "MinGW Makefiles"
- **Make:** `mingw32-make` (same LLVM-mingw bin dir)
- **Python:** for the dev runner script

### Layout

The project builds two targets from the same sources:

| Target | Type | Purpose |
|--------|------|---------|
| `OptiNotch` | Executable | The standalone, self-contained release app |
| `OptiNotch_shared` | Shared library (DLL) | Loaded by `runner.py` during development (corporate AVs block the exe) |

### Building

```powershell
# First-time configure
& "C:\Program Files\CMake\bin\cmake.exe" -B build -G "MinGW Makefiles" `
    "-DCMAKE_CXX_COMPILER=C:\path\to\clang++.exe" `
    "-DCMAKE_MAKE_PROGRAM=C:\path\to\mingw32-make.exe"

# Build
& "C:\Program Files\CMake\bin\cmake.exe" --build build
```

### Running in development

Corporate antivirus blocks `.exe` files, so the dev loop builds the DLL and
loads it via Python `ctypes`:

```powershell
python runner.py
```

`runner.py` builds `libOptiNotch_shared.dll` and calls the exported `run()`,
which **blocks until the notch closes** — that's normal. Before rebuilding, kill
any running instance (the DLL is locked while loaded):

```powershell
Get-Process | Where-Object { $_.Name -match 'python|OptiNotch' } | Stop-Process -Force
& "C:\Program Files\CMake\bin\cmake.exe" --build build
```

### Releasing

```powershell
python tools/make_release.py
```

Builds a Release `OptiNotch.exe` (statically linked, no runtime DLLs, fonts
embedded) and packs `dist/OptiNotch-<ver>.zip` with just the exe, `README.txt`,
and — when found — the real `gcal_credentials.json` (so end users sign in with
their own account).

---

## Configuration

`%APPDATA%\OptiNotch\settings.json` is created on first launch and written on
settings changes:

```json
{
  "monitor": -1,
  "x_offset": 0,
  "hide_hotkey": true,
  "start_with_windows": false,
  "accent_r": 92,
  "accent_g": 147,
  "accent_b": 255,
  "opacity_normal": 240,
  "media_enabled": true,
  "calendar_enabled": true
}
```

| Key | Meaning |
|-----|---------|
| `monitor` | `-1` = primary monitor, `0..N-1` = Nth enumerated monitor |
| `x_offset` | Logical px to shift the notch from the monitor's center |
| `hide_hotkey` | Enable the **Win+Alt** hide hotkey |
| `start_with_windows` | Register the `HKCU\...\Run` autostart entry |
| `accent_r/g/b` | Accent color (0–255 each) used across the UI |
| `opacity_normal` | Notch fill opacity (0–255) |
| `media_enabled` | Start the media (SMTC) poller |
| `calendar_enabled` | Start the Google Calendar poller |

Credentials live at `%APPDATA%\OptiNotch\gcal_credentials.json` (per-user,
imported or bundled) and the OAuth token at `%APPDATA%\OptiNotch\gcal_token.json`.

---

## Project structure

```
src/
├── main.cpp           # D3D11 + DirectComposition init, message loop, run()
├── window/            # window creation, DPI scale, monitor positioning, hover/expand/hide animation
├── ui/                # main_ui (island + clock + halves), settings_ui, setup_ui
├── settings/          # AppSettings (settings.json), autostart registry
├── tray/              # system tray icon + context menu
├── calendar/          # Google Calendar OAuth2 + events, HTTP, JSON
├── media/             # Windows.Media.Control (SMTC) session manager, album art
imgui/                 # vendored Dear ImGui (repo root)
assets/fonts/          # Inter fonts (embedded into the exe via #embed at build)
runner.py              # dev build + run via Python ctypes
tools/                 # make_release.py, README.txt (shipped), gcal_credentials.example.json
CMakeLists.txt         # C++17
```

---

## Technical highlights

- **Rendering:** Direct3D 11 + DirectComposition for per-pixel-alpha transparency
  (not a layered window). The island is drawn with a custom rounded silhouette;
  bottom corners are concave like a real notch.
- **UI:** Dear ImGui (vendored) with Win32 + DX11 backends. The whole UI is laid
  out in **logical px** and scaled by the monitor DPI so it's crisp on any
  display.
- **Threading:** background threads poll the media session and Google Calendar;
  the UI thread never blocks on network/COM.
- **DPI:** fonts are rasterized at scaled sizes and the framebuffer is scaled so
  text stays sharp at 100–200% display scaling.

See `AGENTS.md` in the repo root for detailed engineering notes (build gotchas,
DirectComposition setup, the SMTC async pitfalls, OAuth flow, etc.).
