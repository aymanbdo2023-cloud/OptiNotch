#include "tray.h"
#include "../settings/settings.h"
#include "../window/window.h"
#include "../ui/settings_ui.h"

#include <shellapi.h>
#include <vector>

namespace {

HWND g_hwnd = nullptr;
HICON g_icon = nullptr;

enum MenuId {
    M_SHOW = 1,
    M_SETTINGS,
    M_AUTOSTART,
    M_HOTKEY,
    M_QUIT,
};

// Draw a small island silhouette into a monochrome icon: opaque pixels are
// (AND=0, XOR=1) = black, everything else is (AND=1) = transparent.
HICON make_tray_icon() {
    const int S = 32;
    std::vector<BYTE> andBits((S / 8) * S, 0), orBits((S / 8) * S, 0);
    auto set_pixel = [&](std::vector<BYTE>& b, int x, int y) {
        if (x < 0 || x >= S || y < 0 || y >= S) return;
        b[y * (S / 8) + x / 8] |= (BYTE)(0x80 >> (x % 8));
    };

    const int x0 = 3, x1 = S - 4, y0 = 2, y1 = S - 3;
    const int r = 7, corner = x0 + r;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            bool in = true;
            if (y >= y1 - r) { // rounded bottom corners
                int by = y - (y1 - r);
                int dx_left = corner - x;
                int dx_right = x - (x1 - 1 - corner);
                if (dx_left > 0 && dx_left * dx_left + by * by > r * r) in = false;
                if (dx_right > 0 && dx_right * dx_right + by * by > r * r) in = false;
            }
            if (in) set_pixel(orBits, x, y);
            else    set_pixel(andBits, x, y);
        }
    }

    HBITMAP hbmColor = CreateBitmap(S, S, 1, 1, orBits.data());
    HBITMAP hbmMask = CreateBitmap(S, S, 1, 1, andBits.data());
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmMask = hbmMask;
    ii.hbmColor = hbmColor;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    return icon;
}

void show_menu() {
    AppSettings s = settings_get();
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, M_SHOW, L"Show / Hide notch");
    AppendMenuW(m, MF_STRING, M_SETTINGS, L"Settings\u2026");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, (s.start_with_windows ? MF_STRING | MF_CHECKED : MF_STRING),
        M_AUTOSTART, L"Start with Windows");
    AppendMenuW(m, (s.hide_hotkey ? MF_STRING | MF_CHECKED : MF_STRING),
        M_HOTKEY, L"Win+Alt hides the notch");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | MF_DISABLED | MF_GRAYED, 0, L"Ctrl+Alt+Q quits");
    AppendMenuW(m, MF_STRING, M_QUIT, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    int id = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        pt.x, pt.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0); // dismiss menu
    DestroyMenu(m);

    switch (id) {
    case M_SHOW:
        window_set_hidden(!window_is_hidden());
        break;
    case M_SETTINGS:
        ui_open_settings_panel();
        break;
    case M_AUTOSTART:
        s.start_with_windows = !s.start_with_windows;
        settings_set(s);
        settings_save();
        settings_apply_autostart();
        break;
    case M_HOTKEY:
        s.hide_hotkey = !s.hide_hotkey;
        settings_set(s);
        settings_save();
        break;
    case M_QUIT:
        PostMessageW(g_hwnd, WM_DESTROY, 0, 0);
        break;
    default:
        break;
    }
}

} // namespace

void tray_init(HWND hwnd) {
    g_hwnd = hwnd;
    g_icon = make_tray_icon();

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = TRAY_CALLBACK;
    nid.hIcon = g_icon;
    wcsncpy(nid.szTip, L"OptiNotch", 63);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void tray_shutdown() {
    if (g_hwnd) {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = g_hwnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
    if (g_icon) {
        DestroyIcon(g_icon);
        g_icon = nullptr;
    }
    g_hwnd = nullptr;
}

void tray_handle_message(WPARAM /*wparam*/, LPARAM lparam) {
    UINT msg = (UINT)(lparam & 0xFFFF);
    if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP)
        show_menu();
}
