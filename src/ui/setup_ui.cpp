#include "setup_ui.h"
#include "../calendar/calendar.h"
#include "main_ui.h"

#include <windows.h>
#include <commdlg.h>
#include <string>

namespace {

// Outline pill button centered on `center`. Returns true when clicked.
bool draw_outline_button(ImDrawList* dl, ImVec2 center, const char* id,
    const char* label, ImFont* font, float size) {
    ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, -1.0f, label);
    float bw = ts.x + 24.0f, bh = ts.y + 12.0f;
    ImVec2 p0(center.x - bw * 0.5f, center.y - bh * 0.5f);
    ImVec2 p1(center.x + bw * 0.5f, center.y + bh * 0.5f);

    ImGui::SetCursorScreenPos(p0);
    ImGui::InvisibleButton(id, ImVec2(bw, bh));
    bool hv = ImGui::IsItemHovered();
    bool cl = ImGui::IsItemClicked();

    dl->AddRect(p0, p1, hv ? IM_COL32(255, 255, 255, 200) : IM_COL32(255, 255, 255, 110),
        bh * 0.5f, ImDrawFlags_None, 1.0f);
    dl->AddText(font, size, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f),
        hv ? IM_COL32(255, 255, 255, 255) : IM_COL32(214, 216, 224, 255), label);
    return cl;
}

bool pick_json_file(std::wstring& out) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"JSON files\0*.json\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select your Google OAuth client JSON";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return false;
    out = file;
    return true;
}

} // namespace

void render_cal_setup_widgets(ImDrawList* dl, float ccx, float y0, float cw) {
    static std::string error;

    ImVec2 b1(ccx, y0 + 10.0f);
    if (draw_outline_button(dl, b1, "##setup_cloud", "Google Cloud setup", g_font_normal, 12.0f))
        cal_open_cloud_setup();

    ImVec2 b2(ccx, y0 + 34.0f);
    if (draw_outline_button(dl, b2, "##setup_load", "Load client JSON\u2026", g_font_normal, 12.0f)) {
        std::wstring path;
        if (pick_json_file(path)) {
            std::string err;
            if (cal_import_credentials_file(path, err)) {
                error.clear();
                cal_request_refresh();
            } else {
                error = err;
            }
        }
    }

    float ty = y0 + 58.0f;
    if (!error.empty()) {
        float x = ccx - (cw - 8.0f) * 0.5f;
        ImVec2 p0(x, ty - 2.0f), p1(x + (cw - 8.0f), ty + 30.0f);
        dl->PushClipRect(p0, p1, true);
        dl->AddText(g_font_small, 11.0f, ImVec2(x, ty),
            IM_COL32(255, 170, 90, 255), error.c_str());
        dl->PopClipRect();
    } else {
        const char* hint = "Then press Connect below";
        ImVec2 ts = g_font_small->CalcTextSizeA(11.0f, FLT_MAX, -1.0f, hint);
        dl->AddText(g_font_small, 11.0f, ImVec2(ccx - ts.x * 0.5f, ty),
            IM_COL32(148, 151, 161, 255), hint);
    }
}
