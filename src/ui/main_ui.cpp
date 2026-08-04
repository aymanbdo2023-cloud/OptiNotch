#include "main_ui.h"
#include "../settings/settings.h"
#include "settings_ui.h"
#include "../calendar/calendar.h"
#include "../media/media.h"
#include "../window/window.h"
#include "setup_ui.h"
#include <imgui.h>
#include <cstdio>
#include <Windows.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#include <wincodec.h>

// Per-corner notch shape in px. Top corners FLARE OUTWARD (convex) to merge
// with the screen top; bottom corners are CONCAVE cuts that scale down with
// the island height (capped at RAD_BR/RAD_BL) so they stay rounded when the
// island collapses. Edit these to reshape each corner individually.
const float RAD_TL = 0.0f;   // top-left     outward flare (0 = flush with screen top)
const float RAD_TR = 0.0f;   // top-right    outward flare
const float RAD_BR = 20.0f;  // bottom-right concave cut (max radius, scales with height)
const float RAD_BL = 20.0f;  // bottom-left  concave cut (max radius, scales with height)

static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static ID3D11ShaderResourceView* g_art_srv = nullptr;
static ID3D11Texture2D* g_art_tex = nullptr;
static unsigned int g_art_rev_seen = 0xFFFFFFFFu;
static float g_art_aspect = 1.0f;

static bool ui_debug_enabled() {
    static bool on = [] {
        char buf[2] = {};
        return GetEnvironmentVariableA("OPTINOTCH_MEDIA_DEBUG", buf, 2) > 0;
    }();
    return on;
}

#define UIDBG(fmt, ...) do { \
    if (ui_debug_enabled()) { \
        FILE* _f = fopen("media_dbg.log", "a"); \
        if (_f) { fprintf(_f, fmt "\n", ##__VA_ARGS__); fclose(_f); } \
    } \
} while (0)

// Font sizes match the AddFontFromFileTTF calls in run() (this ImGui build has
// no single ImFont::FontSize; each size is baked separately).
static const float FONT_SMALL_SIZE = 11.0f;
static const float FONT_NORMAL_SIZE = 14.0f;
static const float FONT_CLOCK_SIZE = 32.0f;

// Accent color from settings.json, as an ImGui-packed color.
static ImU32 accent_u32() {
    AppSettings s = settings_get();
    return IM_COL32(s.accent_r, s.accent_g, s.accent_b, 255);
}

static ImU32 accent_hover_u32(ImU32 accent) {
    auto clamp255 = [](int v) { return v > 255 ? 255 : v; };
    return IM_COL32(
        clamp255((int)((accent >> 16) & 255) + 26),
        clamp255((int)((accent >> 8) & 255) + 26),
        clamp255((int)(accent & 255) + 26), 255);
}

static ImU32 with_alpha(ImU32 c, int a) {
    return (c & 0x00FFFFFFu) | ((ImU32)a << 24);
}


static void draw_island_shape(ImDrawList* dl, float ox, float oy, float W, float H, float rTL, 
    float rTR, float rBR, float rBL, float alpha) {

    const float PI = 3.14159265358979f;
    const int N = 16;

    float max_r = fminf(W, H) * 0.5f;
    rTL = fminf(rTL, max_r);
    rTR = fminf(rTR, max_r);
    rBR = fminf(rBR, max_r);
    rBL = fminf(rBL, max_r);

    dl->PathClear();

    // Left edge, up to the start of the top-left flare.
    dl->PathLineTo(ImVec2(ox, oy + rTL));

    // Top-left corner: flare OUTWARD (convex) toward the screen-top corner.
    for (int i = 1; i <= N; ++i) {
        float u = (float)i / (float)N;
        float x = ox + u * u * rTL;
        float y = oy + (1 - u) * (1 - u) * rTL;
        dl->PathLineTo(ImVec2(x, y));
    }

    // Top edge, flush with the screen top.
    dl->PathLineTo(ImVec2(ox + W - rTR, oy));

    // Top-right corner: flare OUTWARD (convex) toward the screen-top corner.
    for (int i = 1; i <= N; ++i) {
        float u = (float)i / (float)N;
        float x = (ox + W) - (1 - u) * (1 - u) * rTR;
        float y = oy + u * u * rTR;
        dl->PathLineTo(ImVec2(x, y));
    }

    // Right edge.
    dl->PathLineTo(ImVec2(ox + W, oy + H - rBR));

    // Bottom-right corner: concave (inward) cut.
    for (int i = 1; i <= N; ++i) {
        float a = (PI * 0.5f) * (float)i / (float)N;
        dl->PathLineTo(ImVec2(ox + W - rBR + rBR * cosf(a), oy + H - rBR + rBR * sinf(a)));
    }

    // Bottom edge.
    dl->PathLineTo(ImVec2(ox + rBL, oy + H));

    // Bottom-left corner: concave (inward) cut.
    for (int i = 1; i <= N; ++i) {
        float a = PI * 0.5f + (PI * 0.5f) * (float)i / (float)N;
        dl->PathLineTo(ImVec2(ox + rBL + rBL * cosf(a), oy + H - rBL + rBL * sinf(a)));
    }

    // Back up the left edge to close.
    dl->PathLineTo(ImVec2(ox, oy + rTL));

    dl->PathFillConcave(ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.07f, alpha)));
    dl->PathStroke(ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.07f * alpha)), ImDrawFlags_Closed, 1.0f);
}

static void push_style() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
}



static void pop_style() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

static std::string w2u(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static void fmt_time(double sec, char* out) {
    if (sec < 0) sec = 0;
    int t = (int)(sec + 0.5);
    int m = t / 60, s = t % 60;
    if (m >= 60) sprintf(out, "%d:%02d:%02d", m / 60, m % 60, s);
    else sprintf(out, "%d:%02d", m, s);
}

static void draw_text_ellipsized(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 col,
    const std::string& text, float max_w) {
    if (text.empty() || max_w <= 0) return;
    if (font->CalcTextSizeA(size, FLT_MAX, -1.0f, text.c_str()).x <= max_w) {
        dl->AddText(font, size, pos, col, text.c_str());
        return;
    }
    std::string t = text;
    while (!t.empty()) {
        t.pop_back();
        if (font->CalcTextSizeA(size, FLT_MAX, -1.0f, (t + "...").c_str()).x <= max_w) break;
    }
    dl->AddText(font, size, pos, col, (t + "...").c_str());
}

// Hold the text visible, then scroll it left until it exits, then loop back.
// Elapsed time is measured from the last notch-open (g_marquee_t0) so the title
// restarts from the beginning every time the island expands.
static double g_marquee_t0 = 0.0;
static void draw_marquee_text(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, float vw,
    ImU32 col, const std::string& text) {
    if (text.empty() || vw <= 0) return;
    float tw = font->CalcTextSizeA(size, FLT_MAX, -1.0f, text.c_str()).x;
    if (tw <= vw) {
        dl->AddText(font, size, pos, col, text.c_str());
        return;
    }
    const float dwell = 1.2f;
    const float speed = 40.0f;
    float t = (float)((double)GetTickCount64() / 1000.0 - g_marquee_t0);
    float period = dwell + (tw + vw + 24.0f) / speed;
    float p = fmodf(t, period);
    float x = pos.x;
    if (p > dwell) x = pos.x - (p - dwell) * speed;

    dl->PushClipRect(ImVec2(pos.x, pos.y - 4.0f), ImVec2(pos.x + vw, pos.y + size + 8.0f), true);
    dl->AddText(font, size, ImVec2(x, pos.y), col, text.c_str());
    dl->PopClipRect();
}

// kind: 0 = skip previous, 1 = play, 2 = pause, 3 = skip next
static bool draw_media_icon(ImDrawList* dl, ImVec2 center, float size, ImU32 col, int kind, const char* id) {
    ImGui::SetCursorScreenPos(ImVec2(center.x - size * 0.5f, center.y - size * 0.5f));
    ImGui::InvisibleButton(id, ImVec2(size, size));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImU32 c = hovered ? IM_COL32(255, 255, 255, 235) : col;

    if (g_font_icons) {
        // Modern vector glyphs (Segoe MDL2 Assets) — crisp at any size.
        if (hovered)
            dl->AddCircleFilled(center, size * 0.6f, IM_COL32(255, 255, 255, 16), 32);
        const char* glyph = kind == 0 ? "\xEE\xA2\x92"   // E892 previous
                         : kind == 1 ? "\xEE\x9D\xA8"    // E768 play
                         : kind == 2 ? "\xEE\x9D\xA9"    // E769 pause
                                     : "\xEE\xA2\x93";   // E893 next
        float gs = size * 0.75f;
        ImVec2 ts = g_font_icons->CalcTextSizeA(gs, FLT_MAX, -1.0f, glyph);
        dl->AddText(g_font_icons, gs, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), c, glyph);
        return clicked;
    }

    float s = size * 0.5f;

    if (kind == 0) { // prev: two left-pointing triangles
        ImVec2 a(center.x - s * 0.2f, center.y - s * 0.7f);
        ImVec2 b(center.x - s * 0.2f, center.y + s * 0.7f);
        ImVec2 c0(center.x - s * 0.9f, center.y);
        dl->AddTriangleFilled(a, b, c0, c);
        ImVec2 a2(center.x + s * 0.5f, center.y - s * 0.7f);
        ImVec2 b2(center.x + s * 0.5f, center.y + s * 0.7f);
        ImVec2 c2(center.x - s * 0.2f, center.y);
        dl->AddTriangleFilled(a2, b2, c2, c);
    } else if (kind == 1) { // play
        dl->AddTriangleFilled(
            ImVec2(center.x + s * 0.55f, center.y),
            ImVec2(center.x - s * 0.45f, center.y - s * 0.8f),
            ImVec2(center.x - s * 0.45f, center.y + s * 0.8f), c);
    } else if (kind == 2) { // pause: two bars
        float bw = size * 0.14f;
        dl->AddRectFilled(ImVec2(center.x - s * 0.78f, center.y - s * 0.8f),
                          ImVec2(center.x - s * 0.78f + bw, center.y + s * 0.8f), c, bw * 0.5f);
        dl->AddRectFilled(ImVec2(center.x + s * 0.78f - bw, center.y - s * 0.8f),
                          ImVec2(center.x + s * 0.78f, center.y + s * 0.8f), c, bw * 0.5f);
    } else { // next: two right-pointing triangles
        ImVec2 a(center.x + s * 0.2f, center.y - s * 0.7f);
        ImVec2 b(center.x + s * 0.2f, center.y + s * 0.7f);
        ImVec2 c0(center.x + s * 0.9f, center.y);
        dl->AddTriangleFilled(a, b, c0, c);
        ImVec2 a2(center.x - s * 0.5f, center.y - s * 0.7f);
        ImVec2 b2(center.x - s * 0.5f, center.y + s * 0.7f);
        ImVec2 c2(center.x + s * 0.2f, center.y);
        dl->AddTriangleFilled(a2, b2, c2, c);
    }
    return clicked;
}

static void draw_progress(ImDrawList* dl, ImVec2 pos, float w, float frac, ImU32 track, ImU32 fill, float h) {
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), track, h * 0.5f);
    if (frac > 0.001f) {
        float fw = w * frac;
        dl->AddRectFilled(pos, ImVec2(pos.x + fw, pos.y + h), fill, h * 0.5f);
    }
}

static bool draw_icon_button(ImDrawList* dl, ImVec2 center, float size, const char* glyph, const char* id, ImU32 col = 0) {
    const float half = 9.0f;
    ImGui::SetCursorScreenPos(ImVec2(center.x - half, center.y - half));
    ImGui::InvisibleButton(id, ImVec2(half * 2.0f, half * 2.0f));
    bool hv = ImGui::IsItemHovered();
    bool cl = ImGui::IsItemClicked();
    if (g_font_icons) {
        if (hv)
            dl->AddRectFilled(ImVec2(center.x - half, center.y - half),
                ImVec2(center.x + half, center.y + half), IM_COL32(255, 255, 255, 18), 6.0f);
        ImVec2 ts = g_font_icons->CalcTextSizeA(size, FLT_MAX, -1.0f, glyph);
        ImU32 c = col ? col : (hv ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 188, 198, 210));
        dl->AddText(g_font_icons, size, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), c, glyph);
    }
    return cl;
}

static bool draw_accent_button(ImDrawList* dl, ImVec2 center, const char* label,
    ImFont* font, float size, ImU32 accent) {
    ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, -1.0f, label);
    float bw = ts.x + 28.0f, bh = ts.y + 14.0f;
    ImVec2 p0(center.x - bw * 0.5f, center.y - bh * 0.5f);
    ImVec2 p1(center.x + bw * 0.5f, center.y + bh * 0.5f);
    ImGui::SetCursorScreenPos(p0);
    ImGui::InvisibleButton("##accent", ImVec2(bw, bh));
    bool hv = ImGui::IsItemHovered();
    bool cl = ImGui::IsItemClicked();
    float rad = bh * 0.5f;
    dl->AddRectFilled(ImVec2(p0.x - 5.0f, p0.y - 5.0f), ImVec2(p1.x + 5.0f, p1.y + 5.0f),
        with_alpha(accent, 28), rad + 5.0f);
    dl->AddRectFilled(ImVec2(p0.x - 2.0f, p0.y - 2.0f), ImVec2(p1.x + 2.0f, p1.y + 2.0f),
        with_alpha(accent, 60), rad + 2.0f);
    dl->AddRectFilled(p0, p1, hv ? accent_hover_u32(accent) : accent, rad);
    dl->AddText(font, size, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f),
        IM_COL32(255, 255, 255, 255), label);
    return cl;
}

static void draw_centered_dot_text(ImDrawList* dl, float cx, float y, ImU32 dot_col,
    const char* text, ImFont* font, float size, ImU32 text_col) {
    ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, -1.0f, text);
    float dot_r = 2.5f;
    float total = dot_r * 2.0f + 5.0f + ts.x;
    float x0 = cx - total * 0.5f;
    dl->AddCircleFilled(ImVec2(x0 + dot_r, y + ts.y * 0.5f), dot_r, dot_col, 10);
    dl->AddText(font, size, ImVec2(x0 + dot_r * 2.0f + 5.0f, y), text_col, text);
}

static void render_left_half(ImDrawList* dl, float W, float H) {
    ImVec2 o = ImGui::GetWindowPos();
    const float MX = 18.0f;
    float x0 = o.x + MX, x1 = o.x + W - MX;
    float cw = x1 - x0;
    const float colw = cw / 7.0f;

    const ImU32 C_SUB = IM_COL32(148, 151, 161, 255);
    const ImU32 C_ACCENT = accent_u32();

    if (!settings_get().calendar_enabled) {
        const char* msg = "Calendar disabled (settings)";
        ImVec2 ts = g_font_small->CalcTextSizeA(FONT_SMALL_SIZE, FLT_MAX, -1.0f, msg);
        dl->AddText(g_font_small, FONT_SMALL_SIZE,
            ImVec2(o.x + (W - ts.x) * 0.5f, o.y + (H - ts.y) * 0.5f), C_SUB, msg);
        return;
    }

    CalendarState cal;
    SYSTEMTIME st;
    GetLocalTime(&st);
    {
        std::lock_guard<std::mutex> lk(g_cal_mutex);
        cal = g_cal;
    }
    if (cal.year == 0) { cal.year = st.wYear; cal.month = st.wMonth - 1; }
    bool is_cur = (cal.year == st.wYear && cal.month == st.wMonth - 1);

    char tbuf[8];
    sprintf(tbuf, "%02d:%02d", st.wHour, st.wMinute);
    ImVec2 tts = g_font_collapsed->CalcTextSizeA(15.0f, FLT_MAX, -1.0f, tbuf);
    float hy = o.y + 15.0f;
    dl->AddText(g_font_collapsed, 15.0f, ImVec2(x0, hy - tts.y * 0.5f),
        IM_COL32(214, 216, 224, 255), tbuf);

    if (g_font_icons) {
        ImU32 rcol = cal.busy ? C_ACCENT : 0;
        if (draw_icon_button(dl, ImVec2(x1 - 9.0f, hy), 11.0f, "\xEE\x9C\xAC", "##cal_refresh", rcol))
            cal_request_refresh();
    }

    char hbuf[64];
    sprintf(hbuf, "%s %d", months[cal.month], cal.year);
    float head_mid = (x0 + tts.x + x1 - 22.0f) * 0.5f;
    ImVec2 hts = g_font_collapsed->CalcTextSizeA(14.0f, FLT_MAX, -1.0f, hbuf);
    ImVec2 hr0(head_mid - hts.x * 0.5f - 8.0f, hy - 10.0f), hr1(head_mid + hts.x * 0.5f + 8.0f, hy + 10.0f);
    ImGui::SetCursorScreenPos(hr0);
    ImGui::InvisibleButton("##cal_title", ImVec2(hr1.x - hr0.x, hr1.y - hr0.y));
    bool title_hv = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) cal_goto_today();
    if (title_hv) dl->AddRectFilled(hr0, hr1, IM_COL32(255, 255, 255, 14), 8.0f);
    if (!is_cur)
        dl->AddCircleFilled(ImVec2(hr0.x + 7.0f, hy), 2.5f, C_ACCENT, 8);
    dl->AddText(g_font_collapsed, 14.0f, ImVec2(head_mid - hts.x * 0.5f, hy - hts.y * 0.5f),
        title_hv ? IM_COL32(255, 255, 255, 255) : IM_COL32(224, 226, 232, 255), hbuf);

    int dim = cal_days_in_month(cal.year, cal.month);
    int anchor = is_cur ? cal.today_day : 1;
    if (cal.selected_day >= 1 && cal.selected_day <= dim) anchor = cal.selected_day;
    int week_start = anchor - cal_day_of_week(cal.year, cal.month, anchor);
    int today_col = is_cur ? cal.today_day - week_start : -1;
    if (today_col < 0 || today_col > 6) today_col = -1;

    static const char* wd[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
    for (int i = 0; i < 7; i++) {
        ImVec2 ts = g_font_small->CalcTextSizeA(11.0f, FLT_MAX, -1.0f, wd[i]);
        dl->AddText(g_font_small, 11.0f,
            ImVec2(x0 + colw * (i + 0.5f) - ts.x * 0.5f, o.y + 30.0f),
            i == today_col ? C_ACCENT : IM_COL32(120, 124, 134, 255), wd[i]);
    }

    bool has_ev[32] = {};
    for (const CalEvent& e : cal.events)
        if (e.day >= 1 && e.day <= 31) has_ev[e.day] = true;

    const float cell_w = 28.0f, cell_h = 24.0f;
    const float hl_w = 20.0f, hl_h = 20.0f;
    float cy0 = o.y + 44.0f;
    for (int i = 0; i < 7; i++) {
        int day = week_start + i;
        float ccx = x0 + colw * (i + 0.5f);
        bool valid = day >= 1 && day <= dim;
        ImVec2 c0(ccx - cell_w * 0.5f, cy0), c1(ccx + cell_w * 0.5f, cy0 + cell_h);
        ImVec2 cc((c0.x + c1.x) * 0.5f, (c0.y + c1.y) * 0.5f);
        ImGui::SetCursorScreenPos(c0);
        ImGui::InvisibleButton(("##d" + std::to_string(i)).c_str(), ImVec2(cell_w, cell_h));
        if (!valid) {
            // Boundary day from the adjacent month: dim context only, not clickable.
            char db[8];
            sprintf(db, "%d", day);
            ImVec2 ts = g_font_small->CalcTextSizeA(14.0f, FLT_MAX, -1.0f, db);
            dl->AddText(g_font_small, 14.0f, ImVec2(cc.x - ts.x * 0.5f, cc.y - ts.y * 0.5f),
                IM_COL32(150, 154, 164, 40), db);
            continue;
        }
        bool hv = ImGui::IsItemHovered();
        bool pressed = ImGui::IsItemActive();
        if (ImGui::IsItemClicked()) cal_select_day(day);
        bool today = (day == cal.today_day && is_cur);
        bool sel = (day == cal.selected_day);
        const float hl_ox = -1.0f;
        ImVec2 p0(cc.x - hl_w * 0.5f + hl_ox, cc.y - hl_h * 0.5f), p1(cc.x + hl_w * 0.5f + hl_ox, cc.y + hl_h * 0.5f);

        if (today) {
            dl->AddRectFilled(p0, p1, C_ACCENT, 6.0f);
            dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 55), 6.0f, ImDrawFlags_None, 1.0f);
        } else if (sel) {
            dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 255), 6.0f, ImDrawFlags_None, 1.5f);
        } else if (hv) {
            dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 20), 6.0f);
        }
        if (pressed && !today) dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 40), 6.0f);

        char db[8];
        sprintf(db, "%d", day);
        ImU32 tc = today ? IM_COL32(20, 24, 34, 255) : (sel ? C_ACCENT : (hv ? IM_COL32(255, 255, 255, 255) : IM_COL32(225, 226, 232, 255)));
        ImVec2 ts = g_font_small->CalcTextSizeA(14.0f, FLT_MAX, -1.0f, db);
        dl->AddText(g_font_small, 14.0f, ImVec2(cc.x - ts.x * 0.5f, cc.y - ts.y * 0.5f), tc, db);
        if (has_ev[day]) {
            ImU32 dc = today ? IM_COL32(255, 255, 255, 230) : C_ACCENT;
            dl->AddRectFilled(ImVec2(cc.x - 3.5f, c1.y - 5.0f), ImVec2(cc.x + 3.5f, c1.y - 2.5f), dc, 1.5f);
        }
    }

    float ey = o.y + 74.0f;
    const float card_h = 30.0f, card_gap = 4.0f;
    float ccx = x0 + cw * 0.5f;

    if (!cal.has_credentials) {
        render_cal_setup_widgets(dl, ccx, ey, cw);
    } else if (!cal.authed) {
        if (draw_accent_button(dl, ImVec2(ccx, ey + 12.0f), "Connect Google Calendar",
                g_font_normal, 12.0f, C_ACCENT))
            cal_start_auth();
        if (!cal.status.empty())
            draw_centered_dot_text(dl, ccx, ey + 34.0f, IM_COL32(92, 147, 255, 220),
                cal.status.c_str(), g_font_small, FONT_SMALL_SIZE, C_SUB);
    } else if (cal.busy && cal.events.empty()) {
        draw_centered_dot_text(dl, ccx, ey + 3.0f, C_ACCENT,
            "Loading\u2026", g_font_small, FONT_SMALL_SIZE, C_SUB);
    } else {
        int filter_day = cal.selected_day != 0 ? cal.selected_day : cal.today_day;
        std::vector<const CalEvent*> day_evs;
        for (const CalEvent& e : cal.events)
            if (e.day == filter_day) day_evs.push_back(&e);

        if (day_evs.empty()) {
            draw_centered_dot_text(dl, ccx, ey + 3.0f, IM_COL32(255, 255, 255, 70),
                "No events", g_font_small, FONT_SMALL_SIZE, C_SUB);
            if (!cal.status.empty())
                draw_centered_dot_text(dl, ccx, ey + 22.0f, IM_COL32(255, 170, 90, 255),
                    cal.status.c_str(), g_font_small, FONT_SMALL_SIZE, C_SUB);
        } else {
            float ev_h = H - (ey - o.y) - 8.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(255, 255, 255, 20));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 90));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 255, 255, 150));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, IM_COL32(255, 255, 255, 200));
            ImGui::SetCursorScreenPos(ImVec2(x0, ey));
            if (ImGui::BeginChild("##evscroll", ImVec2(cw, ev_h))) {
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                int idx = 0;
                for (const CalEvent* ep : day_evs) {
                    if (idx > 0)
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + card_gap);
                    const CalEvent& e = *ep;
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImVec2 c0(p.x, p.y), c1(p.x + cw, p.y + card_h);
                    cdl->AddRectFilled(c0, c1, IM_COL32(255, 255, 255, 10), 6.0f);
                    cdl->AddRectFilled(ImVec2(p.x + 3.0f, p.y + 6.0f), ImVec2(p.x + 5.0f, p.y + card_h - 6.0f),
                        IM_COL32(120, 220, 150, 255), 2.0f);
                    ImGui::InvisibleButton(("##ev" + std::to_string(idx)).c_str(), ImVec2(cw, card_h));
                    bool hv = ImGui::IsItemHovered();
                    if (hv) {
                        cdl->AddRectFilled(c0, c1, IM_COL32(255, 255, 255, 16), 6.0f);
                        cdl->AddRectFilled(ImVec2(p.x + 3.0f, p.y + 6.0f), ImVec2(p.x + 5.0f, p.y + card_h - 6.0f),
                            IM_COL32(140, 235, 170, 255), 2.0f);
                    }
                    std::string time_part = e.when;
                    std::size_t sep = time_part.find(" \xC2\xB7 ");
                    if (sep != std::string::npos)
                        time_part = time_part.substr(sep + 3);
                    float mw = g_font_small->CalcTextSizeA(FONT_SMALL_SIZE, FLT_MAX, -1.0f, time_part.c_str()).x;
                    draw_text_ellipsized(cdl, g_font_collapsed, 12.0f, ImVec2(p.x + 13.0f, p.y + 6.0f),
                        IM_COL32(242, 243, 248, 255), e.summary, cw - 13.0f - 6.0f - mw - 6.0f);
                    cdl->AddText(g_font_small, FONT_SMALL_SIZE, ImVec2(p.x + cw - mw - 6.0f, p.y + 8.0f),
                        IM_COL32(150, 153, 163, 255), time_part.c_str());
                    idx++;
                }
                if (!cal.status.empty()) {
                    float sy = ey + (float)day_evs.size() * (card_h + card_gap);
                    draw_centered_dot_text(cdl, ccx, sy, IM_COL32(255, 170, 90, 255),
                        cal.status.c_str(), g_font_small, FONT_SMALL_SIZE, C_SUB);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        }
    }
}

static void render_right_half(ImDrawList* dl, float W, float H) {
    ImVec2 o = ImGui::GetWindowPos();

    if (!g_media.has_media) {
        const char* msg = "No music playing";
        ImVec2 ts = g_font_normal->CalcTextSizeA(FONT_NORMAL_SIZE, FLT_MAX, -1.0f, msg);
        dl->AddText(g_font_normal, FONT_NORMAL_SIZE,
            ImVec2(o.x + (W - ts.x) * 0.5f, o.y + (H - ts.y) * 0.5f),
            IM_COL32(160, 160, 170, 150), msg);
        return;
    }

    const float M = 16.0f;
    float art_w = 80.0f, art_h = 80.0f;
    if (g_art_aspect > 1.2f) {
        art_h = 70.0f;
        art_w = art_h * g_art_aspect;
        if (art_w > 150.0f) {
            art_w = 150.0f;
            art_h = art_w / g_art_aspect;
        }
    }
    ImVec2 art_p0(o.x + M, o.y + (H - art_h) * 0.5f);
    if (g_art_srv) {
        dl->AddImageRounded((ImTextureID)(uintptr_t)g_art_srv,
            art_p0, ImVec2(art_p0.x + art_w, art_p0.y + art_h),
            ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 8.0f);
    } else {
        dl->AddRectFilled(art_p0, ImVec2(art_p0.x + art_w, art_p0.y + art_h),
            IM_COL32(255, 255, 255, 18), 8.0f);
        if (g_font_icons) {
            const char* note = "\xEE\xA3\x96";
            float ns = 26.0f;
            ImVec2 nts = g_font_icons->CalcTextSizeA(ns, FLT_MAX, -1.0f, note);
            dl->AddText(g_font_icons, ns,
                ImVec2(art_p0.x + (art_w - nts.x) * 0.5f, art_p0.y + (art_h - nts.y) * 0.5f),
                IM_COL32(255, 255, 255, 60), note);
        }
    }

    float tx = art_p0.x + art_w + 12.0f;
    float tcol_w = o.x + W - M - tx;

    // Compact vertical stack (title / artist / status / controls / progress),
    // centered with symmetric top and bottom margins.
    float cy0 = o.y + (H - 102.0f) * 0.5f;

    draw_marquee_text(dl, g_font_collapsed, 14.0f, ImVec2(tx, cy0), tcol_w,
        IM_COL32(245, 245, 248, 255), w2u(g_media.title));

    std::string sub;
    if (!g_media.artist.empty()) sub = w2u(g_media.artist);
    if (!g_media.album.empty()) {
        if (!sub.empty()) sub += "  \xC2\xB7  ";
        sub += w2u(g_media.album);
    }
    draw_text_ellipsized(dl, g_font_small, FONT_SMALL_SIZE, ImVec2(tx, cy0 + 19.0f),
        IM_COL32(170, 170, 180, 255), sub, tcol_w);

    dl->AddText(g_font_small, FONT_SMALL_SIZE, ImVec2(tx, cy0 + 34.0f),
        IM_COL32(255, 255, 255, 90), g_media.is_playing ? "Playing" : "Paused");

    // Transport controls, then progress bar, tucked close underneath.
    float btn = 20.0f;
    float spacing = 32.0f;
    float ctrl_w = spacing * 2.0f + btn;
    float btn_cy = cy0 + 58.0f;
    float row_cy = cy0 + 84.0f;
    float start_cx = tx + (tcol_w - ctrl_w) * 0.5f + btn * 0.5f;

    ImU32 idle = IM_COL32(215, 215, 220, 225);
    ImU32 disabled = IM_COL32(215, 215, 220, 70);

    if (draw_media_icon(dl, ImVec2(start_cx, btn_cy), btn, g_media.can_prev ? idle : disabled, 0, "##prev"))
        media_skip_previous();
    if (draw_media_icon(dl, ImVec2(start_cx + spacing, btn_cy), btn, (g_media.can_play || g_media.can_pause) ? idle : disabled,
        g_media.is_playing ? 2 : 1, "##toggle"))
        media_toggle_play();
    if (draw_media_icon(dl, ImVec2(start_cx + spacing * 2.0f, btn_cy), btn, g_media.can_next ? idle : disabled, 3, "##next"))
        media_skip_next();

    double now = (double)GetTickCount64() / 1000.0;
    double pos = g_media.position_sec;
    if (g_media.is_playing) pos += now - g_media.snapshot_time_sec;
    if (pos < 0) pos = 0;
    if (g_media.duration_sec > 0 && pos > g_media.duration_sec) pos = g_media.duration_sec;

    float pbar_w = tcol_w;
    float frac = g_media.duration_sec > 0 ? (float)(pos / g_media.duration_sec) : 0.0f;
    frac = fminf(1.0f, fmaxf(0.0f, frac));
    ImVec2 bp0(tx, row_cy - 1.5f);
    draw_progress(dl, bp0, pbar_w, frac, IM_COL32(255, 255, 255, 26), IM_COL32(255, 255, 255, 210), 3.0f);
    if (frac > 0.0f) {
        dl->AddCircleFilled(ImVec2(bp0.x + pbar_w * frac, row_cy), 4.0f, IM_COL32(255, 255, 255, 255));
    }

    char tb[32];
    fmt_time(pos, tb);
    dl->AddText(g_font_small, FONT_SMALL_SIZE, ImVec2(bp0.x, row_cy + 7.0f),
        IM_COL32(170, 170, 180, 255), tb);
    fmt_time(g_media.duration_sec, tb);
    float tw = g_font_small->CalcTextSizeA(FONT_SMALL_SIZE, FLT_MAX, -1.0f, tb).x;
    dl->AddText(g_font_small, FONT_SMALL_SIZE, ImVec2(bp0.x + pbar_w - tw, row_cy + 7.0f),
        IM_COL32(170, 170, 180, 255), tb);
}

static void release_art() {
    if (g_art_srv) { g_art_srv->Release(); g_art_srv = nullptr; }
    if (g_art_tex) { g_art_tex->Release(); g_art_tex = nullptr; }
    g_art_aspect = 1.0f;
}

static void update_art_texture() {
    if (g_media.art_rev == g_art_rev_seen) return;
    g_art_rev_seen = g_media.art_rev;
    release_art();
    if (g_media.art.empty() || !g_device) return;

    UIDBG("[ui] art bytes=%u", (unsigned)g_media.art.size());
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        return;

    IWICBitmapDecoder* decoder = nullptr;
    IWICStream* wic_stream = nullptr;
    HRESULT hr = factory->CreateStream(&wic_stream);
    if (SUCCEEDED(hr)) hr = wic_stream->InitializeFromMemory((BYTE*)g_media.art.data(), (DWORD)g_media.art.size());
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(wic_stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (wic_stream) wic_stream->Release();
    UIDBG("[ui] CreateDecoderFromStream hr=0x%08X", (unsigned)hr);
    if (SUCCEEDED(hr)) {
        IWICBitmapFrameDecode* frame = nullptr;
        hr = decoder->GetFrame(0, &frame);
        UIDBG("[ui] GetFrame hr=0x%08X", (unsigned)hr);
        if (SUCCEEDED(hr)) {
            IWICFormatConverter* conv = nullptr;
            hr = factory->CreateFormatConverter(&conv);
            if (SUCCEEDED(hr)) {
                hr = conv->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                    WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
                UIDBG("[ui] Convert.Initialize hr=0x%08X", (unsigned)hr);
                if (SUCCEEDED(hr)) {
                    UINT w = 0, h = 0;
                    hr = conv->GetSize(&w, &h);
                    g_art_aspect = (w > 0 && h > 0) ? (float)w / (float)h : 1.0f;
                    UIDBG("[ui] size=%ux%u hr=0x%08X", w, h, (unsigned)hr);
                    if (SUCCEEDED(hr)) {
                        const UINT cap = 512;
                        UINT cw = w, ch = h;
                        if (w > cap || h > cap) {
                            double f = (double)cap / (double)(w > h ? w : h);
                            cw = (UINT)(w * f);
                            ch = (UINT)(h * f);
                        }
                        IWICBitmapScaler* scaler = nullptr;
                        if (SUCCEEDED(factory->CreateBitmapScaler(&scaler))) {
                            if (SUCCEEDED(scaler->Initialize(conv, cw, ch, WICBitmapInterpolationModeFant))) {
                                UINT row = cw * 4;
                                std::vector<BYTE> px((size_t)row * ch);
                                HRESULT hrc = scaler->CopyPixels(nullptr, row, (UINT)px.size(), px.data());
                                if (SUCCEEDED(hrc)) {
                                    D3D11_TEXTURE2D_DESC td{};
                                    td.Width = cw;
                                    td.Height = ch;
                                    td.MipLevels = 1;
                                    td.ArraySize = 1;
                                    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                                    td.SampleDesc.Count = 1;
                                    td.Usage = D3D11_USAGE_IMMUTABLE;
                                    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                                    D3D11_SUBRESOURCE_DATA sd{};
                                    sd.pSysMem = px.data();
                                    sd.SysMemPitch = row;
                                    HRESULT hrt = g_device->CreateTexture2D(&td, &sd, &g_art_tex);
                                    HRESULT hrs = g_art_tex ? g_device->CreateShaderResourceView(g_art_tex, nullptr, &g_art_srv) : E_FAIL;
                                    UIDBG("[ui] texture hr=0x%08X srv hr=0x%08X (%ux%u)",
                                        (unsigned)hrt, (unsigned)hrs, cw, ch);
                                } else {
                                    UIDBG("[ui] CopyPixels failed hr=0x%08X", (unsigned)hrc);
                                }
                            }
                            scaler->Release();
                        }
                    }
                }
                conv->Release();
            }
            frame->Release();
        }
        decoder->Release();
    }
    factory->Release();
}

void ui_cleanup() {
    release_art();
}

static bool g_was_expanded = false;

void render_ui(bool expanded, int width, int height) {
    update_art_texture();

    if (expanded && !g_was_expanded)
        g_marquee_t0 = (double)GetTickCount64() / 1000.0;
    g_was_expanded = expanded;

    float p = g_progress;
    float iw = NOTCH_WIDTH + (EXPANDED_WIDTH - NOTCH_WIDTH) * p;
    float ih = NOTCH_HEIGHT + (EXPANDED_HEIGHT - NOTCH_HEIGHT) * p;
    float ix = ((float)width - iw) * 0.5f;
    float iy = 0.0f;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
    ImGui::Begin("Notch", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rbr = fminf(RAD_BR, ih * 0.25f);
    float rbl = fminf(RAD_BL, ih * 0.25f);
    draw_island_shape(dl, ix, iy, iw, ih, RAD_TL, RAD_TR, rbr, rbl, g_window_alpha);

    if (expanded) {
        float mx = ix + iw * 0.45f;
        dl->AddRectFilled(ImVec2(mx - 1.0f, iy + 24.0f), ImVec2(mx + 1.0f, iy + ih - 24.0f),
            IM_COL32(255, 255, 255, 40));
    }

    push_style();

    if (expanded) {
        ImGui::SetCursorPos(ImVec2(ix, iy));
        ImGui::BeginChild("island_content", ImVec2(iw, ih), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse);

        if (ui_settings_open()) {
            render_settings_panel(iw, ih);
        } else {
            float left_w = iw * 0.45f;
            float right_w = iw - left_w;

            ImGui::BeginChild("left", ImVec2(left_w, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse);
            render_left_half(ImGui::GetWindowDrawList(), left_w, ih);
            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild("right", ImVec2(right_w, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse);
            render_right_half(ImGui::GetWindowDrawList(), right_w, ih);
            ImGui::EndChild();
        }

        // Gear: opens/closes the settings panel. It lives in its own child
        // window created after the calendar/media halves so it is the top-most
        // hover target; a button added to the parent window would be shadowed
        // by the right-half child and never receive clicks.
        if (g_font_icons) {
            const float gh = 12.0f;
            ImGui::SetCursorScreenPos(ImVec2(ix + iw - 22.0f - gh, iy + 12.0f - gh));
            ImGui::BeginChild("##gear_hit", ImVec2(gh * 2.0f, gh * 2.0f), ImGuiChildFlags_None,
                ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
            if (draw_icon_button(ImGui::GetWindowDrawList(), ImVec2(ix + iw - 22.0f, iy + 12.0f),
                    11.0f, "\xEE\x9C\x93", "##gear")) {
                if (ui_settings_open())
                    ui_close_settings_panel();
                else
                    ui_open_settings_panel();
            }
            ImGui::EndChild();
        }

        ImGui::EndChild(); // island_content

    } else {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char buf[64];
        sprintf(buf, "%02d:%02d", st.wHour, st.wMinute);
        ImGui::PushFont(g_font_collapsed);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        ImGui::SetCursorPos(ImVec2(ix + (iw - ts.x) * 0.5f, iy + (ih - ts.y) * 0.5f));
        ImGui::Text("%s", buf);
        ImGui::PopFont();
    }

    pop_style();
    ImGui::End();
}
