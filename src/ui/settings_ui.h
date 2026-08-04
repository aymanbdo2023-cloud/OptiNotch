#pragma once
#include <imgui.h>

// Rendered inside the expanded notch in place of the calendar/media halves
// whenever the settings panel is open.
void render_settings_panel(float iw, float ih);

void ui_open_settings_panel();
void ui_close_settings_panel();
bool ui_settings_open();
