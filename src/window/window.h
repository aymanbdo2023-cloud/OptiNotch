#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>

extern ID3D11Device *g_device;
extern ID3D11DeviceContext *g_context;
extern IDXGISwapChain *g_swap_chain;
extern ID3D11RenderTargetView * g_target_view;
extern IDCompositionDevice *g_dcomp_device;
extern IDCompositionTarget *g_dcomp_target;
extern IDCompositionVisual *g_dcomp_visual;
extern float g_progress;
extern float g_window_alpha;

constexpr int NOTCH_WIDTH = 120;
constexpr int NOTCH_HEIGHT = 20;
constexpr int EXPANDED_WIDTH = 600;
constexpr int EXPANDED_HEIGHT = 190;

bool create_window(HINSTANCE instance, int width, int height);
HWND get_window_handle();
void destroy_window();
void update_window_animation();

int get_window_width();
int get_window_height();

// DPI scale (physical px per logical px) for the chosen monitor, fixed at startup.
float get_ui_scale();
// Recompute the window's top-center position from the current settings.
void window_apply_position();
// Tray/hotkey hide override: keeps the notch animated away until cleared.
void window_set_hidden(bool hidden);
bool window_is_hidden();