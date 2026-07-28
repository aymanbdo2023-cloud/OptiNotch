#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#ifdef OPTINOTCH_BUILD_AS_DLL
#define OPTINOTCH_API __declspec(dllexport)
#else
#define OPTINOTCH_API
#endif

static HWND g_hwnd = nullptr;
static ID3D11Device *g_device = nullptr;
static ID3D11DeviceContext *g_context = nullptr;
static IDXGISwapChain *g_swap_chain = nullptr;
static ID3D11RenderTargetView *g_target_view = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static bool create_d3d11(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, 1, D3D11_SDK_VERSION,
            &desc, &g_swap_chain, &g_device, nullptr, &g_context)))
        return false;

    ID3D11Texture2D *back_buffer = nullptr;
    if (FAILED(g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
        return false;

    g_device->CreateRenderTargetView(back_buffer, nullptr, &g_target_view);
    back_buffer->Release();
    return g_target_view != nullptr;
}

static void cleanup_d3d11() {
    if (g_target_view) { g_target_view->Release(); g_target_view = nullptr; }
    if (g_swap_chain)  { g_swap_chain->Release();  g_swap_chain = nullptr; }
    if (g_context)     { g_context->Release();      g_context = nullptr; }
    if (g_device)      { g_device->Release();       g_device = nullptr; }
}

extern "C" OPTINOTCH_API int run() {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"OptiNotchClass";

    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"OptiNotch",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             1280, 720, nullptr, nullptr, wc.hInstance, nullptr);

    if (!g_hwnd)
        return 1;

    if (!create_d3d11(g_hwnd)) {
        DestroyWindow(g_hwnd);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!running)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Where the code for my UI should be
        ImGui::ShowDemoWindow(nullptr);

        ImGui::Render();

        const float clear_color[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_context->OMSetRenderTargets(1, &g_target_view, nullptr);
        g_context->ClearRenderTargetView(g_target_view, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap_chain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_d3d11();
    DestroyWindow(g_hwnd);

    return 0;
}

int main() {
    return run();
}
