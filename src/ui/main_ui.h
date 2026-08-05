#pragma once
#include <imgui.h>

extern ImFont* g_font_small;
extern ImFont* g_font_normal;
extern ImFont* g_font_clock;
extern ImFont* g_font_collapsed;
extern ImFont* g_font_icons;

void render_ui(bool expanded, int width, int height);
void ui_cleanup();

// Accent color from settings, as an ImGui-packed ARGB.
ImU32 accent_u32();
