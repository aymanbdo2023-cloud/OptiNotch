#define WIN32_LEAN_AND_MEAN
#include "window/window.h"
#include "ui/main_ui.h"

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

ID3D11Device *g_device = nullptr;
ID3D11DeviceContext *g_context = nullptr;
IDXGISwapChain *g_swap_chain = nullptr;
ID3D11RenderTargetView *g_target_view = nullptr;

ImFont* g_font_small = nullptr;
ImFont* g_font_normal = nullptr;
ImFont* g_font_clock = nullptr;
ImFont* g_font_collapsed = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

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
    if (!create_window(GetModuleHandleW(nullptr), 100, 15))
        return 1;

    if (!create_d3d11(get_window_handle()))
        return 1;

    if (!g_device || !g_context || !g_swap_chain || !g_target_view)
        return 2;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    g_font_small     = io.Fonts->AddFontFromFileTTF("assets/fonts/Inter-Regular.ttf", 11.0f);
    g_font_normal    = io.Fonts->AddFontFromFileTTF("assets/fonts/Inter-Regular.ttf", 14.0f);
    g_font_clock     = io.Fonts->AddFontFromFileTTF("assets/fonts/Inter-SemiBold.ttf", 32.0f);
    g_font_collapsed = io.Fonts->AddFontFromFileTTF("assets/fonts/Inter-SemiBold.ttf", 14.0f);
    io.FontDefault = g_font_normal;

    ImGui_ImplWin32_Init(get_window_handle());
    ImGui_ImplDX11_Init(g_device, g_context);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        update_window_animation();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        int w = get_window_width();
        int h = get_window_height();
        ImGui::NewFrame();
        render_ui(g_progress > 0.5f, w, h);
        ImGui::Render();

        const float clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_context->OMSetRenderTargets(1, &g_target_view, nullptr);
        g_context->ClearRenderTargetView(g_target_view, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap_chain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_d3d11();
    destroy_window();

    return 0;
}

int main() {
    return run();
}
