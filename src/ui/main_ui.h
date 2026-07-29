#pragma once
#include <imgui.h>

extern ImFont* g_font_small;
extern ImFont* g_font_normal;
extern ImFont* g_font_clock;
extern ImFont* g_font_collapsed;

void render_ui(bool expanded, int width, int height);
