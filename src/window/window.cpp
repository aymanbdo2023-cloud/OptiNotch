#include "window.h"
#include "../settings/settings.h"
#include "../ui/settings_ui.h"
#include "../tray/tray.h"
#include <imgui_impl_win32.h>
#include <dwmapi.h>
#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static HWND g_hwnd = nullptr;
static float g_hide = 0.0f;
static int g_win_x = 0;
static int g_base_y = 0;
static bool g_hide_override = false;
float g_progress = 0.0f;
float g_window_alpha = 240.0f / 255.0f;

// Physical px per logical px. Fixed at startup from the chosen monitor's DPI;
// the whole UI (fonts, swapchain, hit-testing) is scaled by this factor.
static float g_ui_scale = 1.0f;

// MinGW's dcomp.h has no IDCompositionVisual3, the interface that carries
// SetOpacity. Fade via the raw vtable instead: IUnknown(0-2) + Visual(3-19) +
// Visual2(20-21) + Visual3(22-24) then SetOpacity(float) at slot 25.
static void set_visual_opacity(IDCompositionVisual* visual, float opacity) {
    if (!visual) return;
    void** vtbl = *(void***)visual;
    using Fn = HRESULT(STDMETHODCALLTYPE*)(void*, float);
    reinterpret_cast<Fn>(vtbl[25])(visual, opacity);
}

int get_window_width() {
    RECT r;
    return GetWindowRect(g_hwnd, &r) ? (r.right - r.left) : 0;
}

int get_window_height() {
    RECT r;
    return GetWindowRect(g_hwnd, &r) ? (r.bottom - r.top) : 0;
}

float get_ui_scale() {
    return g_ui_scale;
}

// ---- monitor selection ----

struct MonitorSearch {
    int want;      // -1 = primary, 0..N-1 = enumerated index
    int cur = 0;
    RECT rect;
    bool found = false;
};

static BOOL CALLBACK monitor_enum_cb(HMONITOR h, HDC, LPRECT, LPARAM lp) {
    MonitorSearch* s = reinterpret_cast<MonitorSearch*>(lp);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(h, &mi))
        return TRUE;

    if (s->want == -1) {
        if (mi.dwFlags & MONITORINFOF_PRIMARY) {
            s->rect = mi.rcWork;
            s->found = true;
            return FALSE;
        }
    } else if (s->cur == s->want) {
        s->rect = mi.rcWork;
        s->found = true;
        return FALSE;
    }
    s->cur++;
    return TRUE;
}

// Work area (excludes taskbar) of the configured monitor. Falls back to the
// primary screen.
static bool monitor_work_area(int index, RECT& out) {
    MonitorSearch s;
    s.want = index;
    EnumDisplayMonitors(nullptr, nullptr, monitor_enum_cb, reinterpret_cast<LPARAM>(&s));
    if (!s.found) {
        RECT primary = {};
        if (!GetClientRect(GetDesktopWindow(), &primary))
            return false;
        out = primary;
        return true;
    }
    out = s.rect;
    return true;
}

// Animated island rect in window (client) coordinates, in physical px. The
// window is fixed at the expanded size, so this is the centered, interpolated
// island, not the whole window. The island is flush with the window's top edge.
static RECT island_window_rect() {
    RECT rc{};
    RECT win;
    if (GetWindowRect(g_hwnd, &win)) {
        int w = win.right - win.left;
        float s = g_ui_scale;
        float iw = (NOTCH_WIDTH + (EXPANDED_WIDTH - NOTCH_WIDTH) * g_progress) * s;
        float ih = (NOTCH_HEIGHT + (EXPANDED_HEIGHT - NOTCH_HEIGHT) * g_progress) * s;
        float ix = ((float)w - iw) * 0.5f;
        rc.left = (LONG)ix;
        rc.top = 0;
        rc.right = (LONG)(ix + iw);
        rc.bottom = (LONG)ih;
    }
    return rc;
}

// True when screen point (sx, sy) is inside the animated island rect.
static bool point_over_island(int sx, int sy) {
    RECT win;
    if (!GetWindowRect(g_hwnd, &win))
        return false;
    RECT island = island_window_rect();
    return sx >= win.left + island.left && sx <= win.left + island.right &&
           sy >= win.top + island.top && sy <= win.top + island.bottom;
}

// Restrict the window's hit-testable region to the island plus a small margin
// (so the anti-aliased silhouette is never clipped). Everything outside the
// region lets mouse input fall through to the apps beneath, so the transparent
// 600x150 area no longer blocks clicks when the notch is collapsed. This is
// the only reliable cross-process click-through for a DComp window:
// LWA_COLORKEY doesn't apply to composition visuals and HTTRANSPARENT only
// works within the same process.
static void update_window_region() {
    if (!g_hwnd)
        return;
    RECT island = island_window_rect();
    const int margin = 2;
    int x1 = island.left - margin;
    int y1 = island.top - margin;
    int x2 = island.right + margin;
    int y2 = island.bottom + margin;
    if (x2 <= x1 || y2 <= y1)
        return;
    HRGN rgn = CreateRectRgn(x1, y1, x2, y2);
    if (rgn)
        SetWindowRgn(g_hwnd, rgn, FALSE);
}

void window_apply_position() {
    if (!g_hwnd)
        return;
    AppSettings s = settings_get();
    RECT rc;
    if (!monitor_work_area(s.monitor_index, rc))
        return;
    int w = get_window_width();
    int centered = rc.left + (rc.right - rc.left - w) / 2;
    g_win_x = centered + s.x_offset;
    // Keep the notch fully visible on its monitor.
    if (g_win_x < rc.left) g_win_x = rc.left;
    if (g_win_x + w > rc.right) g_win_x = rc.right - w;
    g_base_y = rc.top;
    SetWindowPos(g_hwnd, HWND_TOPMOST, g_win_x, g_base_y, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        return 0;
    case TRAY_CALLBACK:
        tray_handle_message(wparam, lparam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool create_window(HINSTANCE instance, int width, int height) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.lpszClassName = L"OptiNotchClass";

    RegisterClassExW(&wc);

    // Scale the fixed logical window by the target monitor's DPI so the notch
    // is the same physical size on high-DPI displays.
    RECT wr;
    if (!monitor_work_area(settings_get().monitor_index, wr))
        GetClientRect(GetDesktopWindow(), &wr);
    HMONITOR hm = MonitorFromRect(&wr, MONITOR_DEFAULTTONEAREST);
    g_ui_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(hm);
    if (g_ui_scale < 1.0f)
        g_ui_scale = 1.0f;
    int phys_w = (int)(width * g_ui_scale + 0.5f);
    int phys_h = (int)(height * g_ui_scale + 0.5f);

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"OptiNotchClass", L"OptiNotch",
        WS_POPUP,
        0, 0, phys_w, phys_h,
        nullptr, nullptr, instance, nullptr
    );

    if (!g_hwnd)
        return false;

    // Remove the 1px DWM frame so the DComp client area is flush with the
    // window's top edge (otherwise the notch sits 1px below the screen top).
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);

    window_apply_position();
    return true;
}

HWND get_window_handle() {
    return g_hwnd;
}

void destroy_window() {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
}

void window_set_hidden(bool hidden) {
    g_hide_override = hidden;
}

bool window_is_hidden() {
    return g_hide_override;
}

void update_window_animation() {
    if (!g_hwnd)
        return;

    // Hide when the Win+Alt hotkey is held (if enabled) or the tray toggle is
    // on. Polled async state so it works without focus.
    AppSettings s = settings_get();
    bool win = (GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000;
    bool alt = GetAsyncKeyState(VK_MENU) & 0x8000;
    bool settings_active = ui_settings_open();
    // Opening settings reveals the notch even if it was hidden via the tray.
    bool want_hidden = (s.hide_hotkey && win && alt) || (g_hide_override && !settings_active);

    float step = ImGui::GetIO().DeltaTime * 2.5f;
    g_hide = want_hidden ? fminf(1.0f, g_hide + step) : fmaxf(0.0f, g_hide - step);

    static int last_y = INT_MIN;
    int phys_h = (int)(EXPANDED_HEIGHT * g_ui_scale + 0.5f);
    int y = g_base_y - (int)(phys_h * g_hide);
    if (y != last_y) {
        last_y = y;
        SetWindowPos(g_hwnd, HWND_TOPMOST, g_win_x, y, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (g_dcomp_visual)
        set_visual_opacity(g_dcomp_visual, 1.0f - g_hide);

    POINT cursor;
    GetCursorPos(&cursor);

    // Hover only the visible island, not the surrounding transparent window.
    // The settings panel also holds the notch open so it stays usable even
    // when the mouse leaves the island.
    bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    bool hovered = point_over_island(cursor.x, cursor.y) || settings_active;

    // Hovered + Ctrl held -> more transparent
    if (hovered && ctrl_pressed) {
        g_window_alpha = s.opacity_hover / 255.0f;
    } else {
        g_window_alpha = s.opacity_normal / 255.0f;
    }

    // Hovered (no Ctrl) -> expand, otherwise shrink. Settings keeps it open.
    if ((hovered && !ctrl_pressed) || (settings_active && !want_hidden)) {
        g_progress += 0.12f;
    } else {
        g_progress -= 0.12f;
    }

    if (g_progress < 0.0f) g_progress = 0.0f;
    if (g_progress > 1.0f) g_progress = 1.0f;

    update_window_region();
}
