#include "window.h"
#include <imgui_impl_win32.h>
#include <dwmapi.h>
#include <iostream>

const float DEFAULT_OPACITY = 240.0f;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static HWND g_hwnd = nullptr;
float g_progress = 0.0f;
float g_window_alpha = DEFAULT_OPACITY / 255.0f;

int get_window_width() {
    RECT r;
    return GetWindowRect(g_hwnd, &r) ? (r.right - r.left) : 0;
}

int get_window_height() {
    RECT r;
    return GetWindowRect(g_hwnd, &r) ? (r.bottom - r.top) : 0;
}

// True when screen point (sx, sy) is inside the animated island rect. The
// window is fixed at the expanded size, so this is the centered, interpolated
// island, not the whole window.
static bool point_over_island(int sx, int sy) {
    RECT rc;
    if (!GetWindowRect(g_hwnd, &rc))
        return false;
    int w = rc.right - rc.left;
    float iw = NOTCH_WIDTH + (EXPANDED_WIDTH - NOTCH_WIDTH) * g_progress;
    float ih = NOTCH_HEIGHT + (EXPANDED_HEIGHT - NOTCH_HEIGHT) * g_progress;
    float ix = ((float)w - iw) * 0.5f;
    return sx >= rc.left + ix && sx <= rc.left + ix + iw &&
           sy >= rc.top && sy <= rc.top + ih;
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

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"OptiNotchClass", L"OptiNotch",
        WS_POPUP,
        0, 0, width, height,
        nullptr, nullptr, instance, nullptr
    );

    if (!g_hwnd)
        return false;

    // Remove the 1px DWM frame so the DComp client area is flush with the
    // window's top edge (otherwise the notch sits 1px below the screen top).
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);

    // Color-keyed layered window: pixels matching the key (black, the
    // transparent background of the swapchain) become click-through for the
    // apps underneath. HTTRANSPARENT can't pass clicks across processes, but
    // the layered-window color key is handled by DWM itself. Black also has
    // R == B, which avoids the known Aero color-key hit-test bug. The island
    // fill is dark gray, so it stays opaque and keeps receiving mouse input.
    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    RECT desktop;
    GetClientRect(GetDesktopWindow(), &desktop);
    int x = (desktop.right - width) / 2;
    SetWindowPos(g_hwnd, HWND_TOPMOST, x, 0, width, height, SWP_SHOWWINDOW);

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

void update_window_animation() {
    if (!g_hwnd)
        return;

    POINT cursor;
    GetCursorPos(&cursor);

    // Hover only the visible island, not the surrounding transparent window.
    bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    bool hovered = point_over_island(cursor.x, cursor.y);

    // Hovered + Ctrl held -> more transparent
    if (hovered && ctrl_pressed) {
        g_window_alpha = 180.0f / 255.0f;
    } else {
        g_window_alpha = DEFAULT_OPACITY / 255.0f;
    }

    // Hovered (no Ctrl) -> expand, otherwise shrink
    if (hovered && !ctrl_pressed) {
        g_progress += 0.12f;
    } else {
        g_progress -= 0.12f;
    }

    if (g_progress < 0.0f) g_progress = 0.0f;
    if (g_progress > 1.0f) g_progress = 1.0f;
}
