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

// The collapsed island is this fraction of the chosen monitor's width (in
// physical px). The UI is laid out in logical px and scaled by get_ui_scale(),
// so the notch, icons, buttons and widgets all scale with the screen's
// resolution instead of its DPI.
constexpr float NOTCH_WIDTH_FRACTION = 0.06f;
// Lower bound for get_ui_scale() so very small screens don't get microscopic.
constexpr float MIN_UI_SCALE = 0.7f;

// Global quit hotkey id: Ctrl+Alt+Q (see create_window / WM_HOTKEY in
// window.cpp). Quits the app cleanly even when hidden or unfocused.
constexpr UINT QUIT_HOTKEY_ID = 1;

bool create_window(HINSTANCE instance, int width, int height);
HWND get_window_handle();
void destroy_window();
void update_window_animation();

int get_window_width();
int get_window_height();

// Scale (physical px per logical px) for the chosen monitor, fixed at startup
// from the monitor's resolution (NOTCH_WIDTH_FRACTION of its width).
float get_ui_scale();
// Recompute the window's top-center position from the current settings.
void window_apply_position();
// Tray/hotkey hide override: keeps the notch animated away until cleared.
void window_set_hidden(bool hidden);
bool window_is_hidden();