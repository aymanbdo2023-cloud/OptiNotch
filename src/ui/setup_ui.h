#pragma once
#include <imgui.h>

// First-run helper shown in the calendar area when no Google OAuth
// credentials exist yet. Walks the user through creating a Cloud project and
// importing the downloaded client JSON.
void render_cal_setup_widgets(ImDrawList* dl, float ccx, float y0, float cw);
