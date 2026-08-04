#pragma once
#include <windows.h>

// Custom message the tray icon posts to the notch window; window.cpp forwards
// it to tray_handle_message() from its wnd_proc.
constexpr UINT TRAY_CALLBACK = WM_APP + 1;

void tray_init(HWND hwnd);
void tray_shutdown();
void tray_handle_message(WPARAM wparam, LPARAM lparam);
