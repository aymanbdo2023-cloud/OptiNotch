#include "window.h"
#include <imgui_impl_win32.h>
#include <iostream>


const int NOTCH_WIDTH = 120;
const int NOTCH_HEIGHT = 20;
const int EXPANDED_WIDTH = 600; 
const int EXPANDED_HEIGHT = 150; 
static bool expanded = false;


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static HWND g_hwnd = nullptr;
float g_progress = 0.0f;

int get_window_width() {
    RECT r;
    return GetWindowRect(g_hwnd, &r) ? (r.right - r.left) : 0;
}

int get_window_height() {
    RECT r;
    return GetWindowRect(g_hwnd, &r) ? (r.bottom - r.top) : 0;
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

    HRGN region = CreateRoundRectRgn(0, 0, width, height, 2, 4);
    SetWindowRgn(g_hwnd, region, TRUE);

    SetLayeredWindowAttributes(g_hwnd, 0, 220, LWA_ALPHA);
    RECT desktop;
    GetClientRect(GetDesktopWindow(), &desktop);
    int x = (desktop.right - width) / 2;
    SetWindowPos(g_hwnd, HWND_TOPMOST, x, 3, width, height, SWP_SHOWWINDOW);

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
    RECT rect;
    GetWindowRect(g_hwnd, &rect);

    bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    bool hovered = (cursor.x >= rect.left && cursor.x <= rect.right &&
                    cursor.y >= 0 && cursor.y <= rect.bottom);


    // Defining the behaviour of the window based on different events 
    // When hovered + ctrl pressed
    if (hovered && ctrl_pressed) {
        // The 180 is the traslucence value (255 is fully opaque)
        SetLayeredWindowAttributes(g_hwnd, 0, 180, LWA_ALPHA); 
    }

    // When not hovered + ctrl not pressed
    if (!hovered && !ctrl_pressed) {
        SetLayeredWindowAttributes(g_hwnd, 0, 220, LWA_ALPHA); 
    }

    // When not hovered + ctrl pressed
    if (!hovered && ctrl_pressed) {
        SetLayeredWindowAttributes(g_hwnd, 0, 220, LWA_ALPHA); 
    }
    
    // When hovered + ctrl not pressed
    if (hovered && !ctrl_pressed) {
        SetLayeredWindowAttributes(g_hwnd, 0, 220, LWA_ALPHA); 
        g_progress += 0.04f;
        expanded = true;
    }
    else {
        g_progress -= 0.05f;
        expanded = false;
    }

    if (g_progress < 0.0f) g_progress = 0.0f;
    if (g_progress > 1.0f) g_progress = 1.0f;
    

    int w = NOTCH_WIDTH + (int)((EXPANDED_WIDTH - NOTCH_WIDTH) * g_progress);
    int h = NOTCH_HEIGHT + (int)((EXPANDED_HEIGHT - NOTCH_HEIGHT) * g_progress);

    static int last_w = 0, last_h = 0;
    if (w == last_w && h == last_h)
        return;
    last_w = w; last_h = h;

    RECT desktop;
    GetClientRect(GetDesktopWindow(), &desktop);
    int x = (desktop.right - w) / 2;

    // Made is so that when the window is expanded there is no padding at the top of the screen
    if (expanded) {
        SetWindowPos(g_hwnd, HWND_TOPMOST, x, 0, w, h, SWP_SHOWWINDOW);
    } else {
        SetWindowPos(g_hwnd, HWND_TOPMOST, x, 3, w, h, SWP_SHOWWINDOW);
    }

    HRGN region = CreateRoundRectRgn(0, 0, w, h, 5, 5);
    SetWindowRgn(g_hwnd, region, TRUE);

    if (g_context && g_swap_chain && g_device) {
        g_context->OMSetRenderTargets(0, nullptr, nullptr);
        if (g_target_view) {
            g_target_view->Release();
            g_target_view = nullptr;
        }
        g_context->Flush();

        g_swap_chain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);

        ID3D11Texture2D *back_buffer = nullptr;
        if (SUCCEEDED(g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
            g_device->CreateRenderTargetView(back_buffer, nullptr, &g_target_view);
            back_buffer->Release();
        }

        D3D11_VIEWPORT vp{};
        vp.Width = (FLOAT)w;
        vp.Height = (FLOAT)h;
        vp.MaxDepth = 1.0f;
        g_context->RSSetViewports(1, &vp);
    }
}