#pragma once
#include <windows.h>
#include <d3d11.h>

extern ID3D11Device *g_device;
extern ID3D11DeviceContext *g_context;
extern IDXGISwapChain *g_swap_chain;
extern ID3D11RenderTargetView * g_target_view;
extern float g_progress;

bool create_window(HINSTANCE instance, int width, int height);
HWND get_window_handle();
void destroy_window();
void update_window_animation();


int get_window_width();
int get_window_height();