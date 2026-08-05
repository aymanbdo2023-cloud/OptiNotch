#include "settings_ui.h"
#include "../settings/settings.h"
#include "../window/window.h"
#include "../calendar/calendar.h"
#include "main_ui.h"

#include <cmath>
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

// Monitor list in EnumDisplayMonitors order, with the primary monitor labeled
// "Primary" regardless of its position in the enumeration (so it is not listed
// twice, once as "Primary" and once as an index entry).
struct MonitorList {
    std::vector<std::string> names;
    int primary = 0;
};

MonitorList monitor_list() {
    MonitorList ml;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR h, HDC, LPRECT, LPARAM lp) -> BOOL {
        auto* data = reinterpret_cast<MonitorList*>(lp);
        int cur = (int)data->names.size();
        MONITORINFOEXW mi = {};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(h, &mi)) {
            bool prim = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
            data->names.push_back(prim ? "Primary" : w2u(mi.szDevice));
            if (prim) data->primary = cur;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ml));
    if (ml.primary < 0) ml.primary = 0;
    return ml;
}

std::string monitor_label(int index) {
    MonitorList ml = monitor_list();
    if (ml.names.empty()) return "Primary";
    if (index < 0) index = ml.primary;                                 // primary
    if (index >= (int)ml.names.size()) return ml.names.back();
    return ml.names[index];
}

} // namespace

void ui_open_settings_panel()  { g_settings_open = true; }
void ui_close_settings_panel() { g_settings_open = false; g_xoff_dragging = false; }
bool ui_settings_open()        { return g_settings_open; }

void render_settings_panel(float iw, float ih) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetWindowPos();
    const float mx = 16.0f;
    float x0 = o.x + mx, x1 = o.x + iw - mx;
    float cw = x1 - x0;
    float y = o.y + 14.0f;

    const ImU32 accent = accent_u32();
    const ImU32 accent_dim = (accent & 0x00FFFFFFu) | (28u << 24);

    // Title: gear glyph + "Settings" + accent underline.
    float tx = x0;
    if (g_font_icons) {
        ImVec2 gs = g_font_icons->CalcTextSizeA(13.0f, FLT_MAX, -1.0f, "\xEE\x9C\x93");
        dl->AddText(g_font_icons, 13.0f, ImVec2(tx, y - gs.y * 0.5f),
            IM_COL32(255, 255, 255, 150), "\xEE\x9C\x93");
        tx += gs.x + 7.0f;
    }
    const char* title = "Settings";
    ImVec2 tts = g_font_collapsed->CalcTextSizeA(15.0f, FLT_MAX, -1.0f, title);
    dl->AddText(g_font_collapsed, 15.0f, ImVec2(tx, y - tts.y * 0.5f),
        IM_COL32(238, 239, 243, 255), title);
    dl->AddRectFilled(ImVec2(x0, y + 10.0f), ImVec2(x1, y + 11.0f), accent_dim, 1.0f);

    y += 22.0f;

    // Scoped modern styling: rounded translucent inputs, accent grabs/checks.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 9));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(255, 255, 255, 20));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 90));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 255, 255, 150));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, IM_COL32(255, 255, 255, 200));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 255, 255, 12));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(255, 255, 255, 22));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(255, 255, 255, 30));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, accent);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, accent);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 255, 255, 14));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 26));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 36));
    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(255, 255, 255, 16));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(255, 255, 255, 28));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(255, 255, 255, 36));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24, 25, 33, 246));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 34));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(214, 216, 224, 255));

    float avail_h = (o.y + ih - 8.0f) - y;
    ImGui::SetCursorScreenPos(ImVec2(x0, y));
    if (ImGui::BeginChild("##settings_scroll", ImVec2(cw, avail_h))) {
        AppSettings s = settings_get();
        bool changed = false;
        bool reposition = false;
        bool autostart_changed = false;
        const float lbl = 126.0f;
        const float item_w = cw - lbl;

        auto section = [&](const char* label) {
            ImGui::Dummy(ImVec2(0, 2));
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImGui::PushFont(g_font_collapsed);
            ImVec2 sts = ImGui::CalcTextSize(label);
            ImGui::Text("%s", label);
            ImGui::PopFont();
            dl->AddLine(ImVec2(x0 + sts.x + 8.0f, p0.y + sts.y * 0.5f), ImVec2(x1, p0.y + sts.y * 0.5f),
                IM_COL32(255, 255, 255, 26), 1.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
        };

        // Subtle rounded card behind each setting row.
        auto row = [&]() {
            ImVec2 p = ImGui::GetCursorScreenPos();
            float h = ImGui::GetFrameHeightWithSpacing();
            dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + cw, p.y + h),
                IM_COL32(255, 255, 255, 7), 8.0f);
        };

        // ---- Display ----
        section("Display");
        row();
        ImGui::Text("Monitor");
        ImGui::SameLine(lbl);
        char mlabel[64];
        sprintf(mlabel, "%s", monitor_label(s.monitor_index).c_str());
        ImGui::SetNextItemWidth(item_w);
        if (ImGui::BeginCombo("##mon", mlabel)) {
            MonitorList ml = monitor_list();
            for (int i = 0; i < (int)ml.names.size(); i++) {
                bool is_sel = (s.monitor_index == i) || (s.monitor_index < 0 && i == ml.primary);
                if (ImGui::Selectable(ml.names[i].c_str(), is_sel)) {
                    s.monitor_index = (i == ml.primary) ? -1 : i;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        // Horizontal offset. The value persists live, but the notch only moves
        // on release (see g_xoff_dragging above).
        row();
        ImGui::Text("Position offset");
        ImGui::SameLine(lbl);
        ImGui::SetNextItemWidth(item_w);
        ImGui::SliderInt("##xoff", &s.x_offset, -400, 400);
        if (ImGui::IsItemEdited()) changed = true;
        bool xoff_active = ImGui::IsItemActive();
        bool xoff_released = g_xoff_dragging && !xoff_active;
        g_xoff_dragging = xoff_active;
        if (xoff_released) reposition = true;

        // ---- Behavior ----
        section("Behavior");
        row();
        if (ImGui::Checkbox("Win+Alt hides the notch", &s.hide_hotkey)) changed = true;
        row();
        if (ImGui::Checkbox("Start with Windows", &s.start_with_windows)) {
            changed = true;
            autostart_changed = true;
        }

        // ---- Appearance ----
        section("Appearance");
        row();
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

        row();
        ImGui::Text("Opacity");
        ImGui::SameLine(lbl);
        ImGui::SetNextItemWidth(item_w);
        if (ImGui::SliderFloat("##opn", &s.opacity_normal, 80.0f, 255.0f, "%.0f")) changed = true;

        if (changed) {
            settings_set(s);
            settings_save();
            if (autostart_changed)
                settings_apply_autostart();
        }
        if (reposition || (changed && !g_xoff_dragging))
            window_apply_position();

        // ---- Account ----
        section("Account");
        CalendarState cst;
        {
            std::lock_guard<std::mutex> lk(g_cal_mutex);
            cst = g_cal;
        }
        if (cst.has_credentials) {
            row();
            ImGui::Text("Google Calendar");
            if (cst.authed) {
                ImGui::SameLine(lbl);
                ImGui::SetNextItemWidth(item_w);
                if (ImGui::Button("Sign out", ImVec2(item_w, 0)))
                    cal_sign_out();
                ImGui::TextDisabled("Press \"Connect Google Calendar\" in the\nnotch to sign in with another account.");
            } else {
                ImGui::SameLine(lbl);
                ImGui::SetNextItemWidth(item_w);
                if (ImGui::Button("Connect Google Calendar", ImVec2(item_w, 0)))
                    cal_start_auth();
            }
        }

        // ---- Done: full-width accent pill ----
        ImGui::Dummy(ImVec2(0, 6));
        ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
        ImVec4 ac_h = ImVec4(fminf(ac.x * 1.15f, 1.0f), fminf(ac.y * 1.15f, 1.0f), fminf(ac.z * 1.15f, 1.0f), 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ac);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ac_h);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ac_h);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 13.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 9));
        if (ImGui::Button("Done", ImVec2(cw, 0)))
            ui_close_settings_panel();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }
    ImGui::EndChild();

    ImGui::PopStyleColor(19);
    ImGui::PopStyleVar(6);
}
