#define WIN32_LEAN_AND_MEAN
#include "window/window.h"
#include "ui/main_ui.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <stdio.h>
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
IDCompositionDevice *g_dcomp_device = nullptr;
IDCompositionTarget *g_dcomp_target = nullptr;
IDCompositionVisual *g_dcomp_visual = nullptr;

ImFont* g_font_small = nullptr;
ImFont* g_font_normal = nullptr;
ImFont* g_font_clock = nullptr;
ImFont* g_font_collapsed = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static bool create_d3d11(HWND hwnd) {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, 1, D3D11_SDK_VERSION,
        &g_device, nullptr, &g_context);
    if (FAILED(hr)) {
        fprintf(stderr, "D3D11CreateDevice failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    IDXGIDevice *dxgi_device = nullptr;
    hr = g_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    if (FAILED(hr)) {
        fprintf(stderr, "QueryInterface IDXGIDevice failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    hr = DCompositionCreateDevice(dxgi_device, IID_PPV_ARGS(&g_dcomp_device));
    if (FAILED(hr)) {
        fprintf(stderr, "DCompositionCreateDevice failed: 0x%08X\n", (unsigned)hr);
        dxgi_device->Release();
        return false;
    }

    hr = g_dcomp_device->CreateTargetForHwnd(hwnd, TRUE, &g_dcomp_target);
    if (FAILED(hr)) {
        fprintf(stderr, "CreateTargetForHwnd failed: 0x%08X\n", (unsigned)hr);
        dxgi_device->Release();
        return false;
    }

    hr = g_dcomp_device->CreateVisual(&g_dcomp_visual);
    if (FAILED(hr)) {
        fprintf(stderr, "CreateVisual failed: 0x%08X\n", (unsigned)hr);
        dxgi_device->Release();
        return false;
    }

    IDXGIAdapter *adapter = nullptr;
    hr = dxgi_device->GetAdapter(&adapter);
    if (FAILED(hr)) {
        fprintf(stderr, "GetAdapter failed: 0x%08X\n", (unsigned)hr);
        dxgi_device->Release();
        return false;
    }

    IDXGIFactory2 *factory2 = nullptr;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory2));
    if (FAILED(hr)) {
        fprintf(stderr, "GetParent IDXGIFactory2 failed: 0x%08X\n", (unsigned)hr);
        adapter->Release();
        dxgi_device->Release();
        return false;
    }

    adapter->Release();
    dxgi_device->Release();

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.BufferCount = 2;
    desc.Width = EXPANDED_WIDTH;
    desc.Height = EXPANDED_HEIGHT;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    IDXGISwapChain1 *swap_chain1 = nullptr;
    hr = factory2->CreateSwapChainForComposition(g_device, &desc, nullptr, &swap_chain1);
    if (FAILED(hr)) {
        fprintf(stderr, "CreateSwapChainForComposition failed: 0x%08X\n", (unsigned)hr);
        factory2->Release();
        return false;
    }
    g_swap_chain = swap_chain1;
    factory2->Release();

    hr = g_dcomp_visual->SetContent(g_swap_chain);
    if (FAILED(hr)) {
        fprintf(stderr, "SetContent failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    hr = g_dcomp_target->SetRoot(g_dcomp_visual);
    if (FAILED(hr)) {
        fprintf(stderr, "SetRoot failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    hr = g_dcomp_device->Commit();
    if (FAILED(hr)) {
        fprintf(stderr, "Commit failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    ID3D11Texture2D *back_buffer = nullptr;
    hr = g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr)) {
        fprintf(stderr, "GetBuffer failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    g_device->CreateRenderTargetView(back_buffer, nullptr, &g_target_view);
    back_buffer->Release();
    return g_target_view != nullptr;
}

static void cleanup_d3d11() {
    if (g_target_view)   { g_target_view->Release();   g_target_view = nullptr; }
    if (g_swap_chain)    { g_swap_chain->Release();    g_swap_chain = nullptr; }
    if (g_dcomp_visual)  { g_dcomp_visual->Release();  g_dcomp_visual = nullptr; }
    if (g_dcomp_target)  { g_dcomp_target->Release();  g_dcomp_target = nullptr; }
    if (g_dcomp_device)  { g_dcomp_device->Release();  g_dcomp_device = nullptr; }
    if (g_context)       { g_context->Release();       g_context = nullptr; }
    if (g_device)        { g_device->Release();        g_device = nullptr; }
}

extern "C" OPTINOTCH_API int run() {
    if (!create_window(GetModuleHandleW(nullptr), EXPANDED_WIDTH, EXPANDED_HEIGHT))
        return 1;

    if (!create_d3d11(get_window_handle()))
        return 2;

    if (!g_device || !g_context || !g_swap_chain || !g_target_view)
        return 3;

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
        if (g_dcomp_device)
            g_dcomp_device->Commit();
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