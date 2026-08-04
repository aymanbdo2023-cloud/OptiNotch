#include "settings_ui.h"
#include "../settings/settings.h"
#include "../window/window.h"
#include "../calendar/calendar.h"
#include "main_ui.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

bool g_settings_open = false;

// True while the position-offset slider is held down. The notch must not
// reposition while it is dragged: moving the window slides the slider itself
// under the cursor, feeding back into the value and racing it to the extreme.
bool g_xoff_dragging = false;

std::string w2u(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

int monitor_count() {
    int count = 0;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR, HDC, LPRECT, LPARAM lp) -> BOOL {
        ++(*reinterpret_cast<int*>(lp));
        return TRUE;
    }, reinterpret_cast<LPARAM>(&count));
    return count;
}

std::string monitor_label(int index) {
    std::vector<std::string> names;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR h, HDC, LPRECT, LPARAM lp) -> BOOL {
        auto* v = reinterpret_cast<std::vector<std::string>*>(lp);
        MONITORINFOEXW mi = {};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(h, &mi)) {
            if (mi.dwFlags & MONITORINFOF_PRIMARY)
                v->push_back("Primary");
            else
                v->push_back(w2u(mi.szDevice));
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&names));

    if (names.empty()) return "Primary";
    if (index < 0) return names[0];                                   // primary
    if (index >= (int)names.size()) return names.back();
    return names[index];
}

} // namespace

void ui_open_settings_panel()  { g_settings_open = true; }
void ui_close_settings_panel() { g_settings_open = false; g_xoff_dragging = false; }
bool ui_settings_open()        { return g_settings_open; }

void render_settings_panel(float iw, float ih) {
    ImVec2 o = ImGui::GetWindowPos();
    const float mx = 16.0f;
    float x0 = o.x + mx, x1 = o.x + iw - mx;
    float cw = x1 - x0;
    float y = o.y + 12.0f;

    ImGui::SetCursorScreenPos(ImVec2(x0, y));
    ImGui::Text("Settings");
    y += 24.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(255, 255, 255, 20));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 90));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 255, 255, 150));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, IM_COL32(255, 255, 255, 200));

    float avail_h = (o.y + ih - 8.0f) - y;
    ImGui::SetCursorScreenPos(ImVec2(x0, y));
    if (ImGui::BeginChild("##settings_scroll", ImVec2(cw, avail_h))) {
        AppSettings s = settings_get();
        bool changed = false;
        bool reposition = false;
        bool autostart_changed = false;
        const float lbl = 118.0f;
        const float item_w = cw - lbl;

        // Monitor
        ImGui::Text("Monitor");
        ImGui::SameLine(lbl);
        char mlabel[64];
        sprintf(mlabel, "%s", monitor_label(s.monitor_index).c_str());
        ImGui::SetNextItemWidth(item_w);
        if (ImGui::BeginCombo("##mon", mlabel)) {
            int total = monitor_count();
            for (int i = -1; i < total; i++) {
                char item[64];
                sprintf(item, "%s", monitor_label(i).c_str());
                if (ImGui::Selectable(item, s.monitor_index == i)) {
                    s.monitor_index = i;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        // Horizontal offset. The value persists live, but the notch only moves
        // on release (see g_xoff_dragging above).
        ImGui::Text("Position offset");
        ImGui::SameLine(lbl);
        ImGui::SetNextItemWidth(item_w);
        ImGui::SliderInt("##xoff", &s.x_offset, -400, 400);
        if (ImGui::IsItemEdited()) changed = true;
        bool xoff_active = ImGui::IsItemActive();
        bool xoff_released = g_xoff_dragging && !xoff_active;
        g_xoff_dragging = xoff_active;
        if (xoff_released) reposition = true;

        // Toggles
        if (ImGui::Checkbox("Win+Alt hides the notch", &s.hide_hotkey)) changed = true;
        if (ImGui::Checkbox("Start with Windows", &s.start_with_windows)) {
            changed = true;
            autostart_changed = true;
        }

        // Accent color
        ImGui::Text("Accent");
        ImGui::SameLine(lbl);
        float col[3] = { s.accent_r / 255.0f, s.accent_g / 255.0f, s.accent_b / 255.0f };
        ImGui::SetNextItemWidth(item_w);
        if (ImGui::ColorEdit3("##accent", col, ImGuiColorEditFlags_NoInputs)) {
            s.accent_r = (int)(col[0] * 255.0f + 0.5f);
            s.accent_g = (int)(col[1] * 255.0f + 0.5f);
            s.accent_b = (int)(col[2] * 255.0f + 0.5f);
            changed = true;
        }

        // Opacity
        ImGui::Text("Opacity");
        ImGui::SameLine(lbl);
        ImGui::SetNextItemWidth(item_w);
        if (ImGui::SliderFloat("##opn", &s.opacity_normal, 80.0f, 255.0f, "%.0f")) changed = true;

        ImGui::Text("Ctrl-hover opacity");
        ImGui::SameLine(lbl);
        ImGui::SetNextItemWidth(item_w);
        if (ImGui::SliderFloat("##oph", &s.opacity_hover, 80.0f, 255.0f, "%.0f")) changed = true;

        if (changed) {
            settings_set(s);
            settings_save();
            if (autostart_changed)
                settings_apply_autostart();
        }
        if (reposition || (changed && !g_xoff_dragging))
            window_apply_position();

        // Google Calendar account: sign out to switch to another account, or
        // (re)connect when signed out. The first-run wizard in the notch only
        // appears when no credentials are bundled/imported at all.
        ImGui::Dummy(ImVec2(0, 8));
        CalendarState cst;
        {
            std::lock_guard<std::mutex> lk(g_cal_mutex);
            cst = g_cal;
        }
        if (cst.has_credentials) {
            if (cst.authed) {
                ImGui::Text("Google Calendar");
                if (ImGui::Button("Sign out", ImVec2(item_w, 0)))
                    cal_sign_out();
                ImGui::TextDisabled("Press \"Connect Google Calendar\" in the\nnotch to sign in with another account.");
            } else {
                ImGui::Text("Google Calendar");
                if (ImGui::Button("Connect Google Calendar", ImVec2(item_w, 0)))
                    cal_start_auth();
            }
        }

        ImGui::Dummy(ImVec2(0, 6));
        if (ImGui::Button("Done"))
            ui_close_settings_panel();
    }
    ImGui::EndChild();

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}
