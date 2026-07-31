#include "main_ui.h"
#include <imgui.h>
#include <cstdio>
#include <Windows.h>
#include <math.h>
#include "../window/window.h"

// Per-corner notch shape in px. Top corners FLARE OUTWARD (convex) to merge
// with the screen top; bottom corners are CONCAVE cuts that scale down with
// the island height (capped at RAD_BR/RAD_BL) so they stay rounded when the
// island collapses. Edit these to reshape each corner individually.
const float RAD_TL = 0.0f;   // top-left     outward flare (0 = flush with screen top)
const float RAD_TR = 0.0f;   // top-right    outward flare
const float RAD_BR = 32.0f;  // bottom-right concave cut (max radius, scales with height)
const float RAD_BL = 32.0f;  // bottom-left  concave cut (max radius, scales with height)

static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


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

    dl->PathFillConcave(ImGui::GetColorU32(ImVec4(0.02f, 0.04f, 0.05f, alpha)));
    dl->PathStroke(ImGui::GetColorU32(ImVec4(0.35f, 0.9f, 1.0f, 0.10f * alpha)), ImDrawFlags_Closed, 1.0f);
}

// --- Liquid-crystal (LCD) seven-segment rendering ---------------------------
// Segments are drawn as rounded rects. Unlit segments show as faint "ghost"
// elements (as on a real LCD backlight); lit segments get a soft glow halo.

static void draw_lcd_segment(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding, ImU32 col) {
    dl->PathRect(min, max, rounding);
    dl->PathFillConcave(col);
}

static void draw_lcd_digit(ImDrawList* dl, char digit, float x, float y, float W, float H, float alpha) {
    static const unsigned char seg_mask[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    unsigned char m = (digit >= '0' && digit <= '9') ? seg_mask[digit - '0'] : 0x7F;

    float t = H * 0.14f;   // segment thickness
    float g = t * 0.45f;   // gap between segments
    float hc = H * 0.5f;

    const ImVec2 S[7][2] = {
        { ImVec2(x + g, y), ImVec2(x + W - g, y + t) },                        // A top
        { ImVec2(x + W - t, y + g), ImVec2(x + W, y + hc - g * 0.5f) },        // B upper-right
        { ImVec2(x + W - t, y + hc + g * 0.5f), ImVec2(x + W, y + H - g) },    // C lower-right
        { ImVec2(x + g, y + H - t), ImVec2(x + W - g, y + H) },                // D bottom
        { ImVec2(x, y + hc + g * 0.5f), ImVec2(x + t, y + H - g) },            // E lower-left
        { ImVec2(x, y + g), ImVec2(x + t, y + hc - g * 0.5f) },                // F upper-left
        { ImVec2(x + g, y + hc - t * 0.5f), ImVec2(x + W - g, y + hc + t * 0.5f) }, // G middle
    };

    float rounding = t * 0.35f;
    ImU32 ghost = ImGui::GetColorU32(ImVec4(0.0f, 0.34f, 0.45f, 0.10f * alpha));
    ImU32 glow  = ImGui::GetColorU32(ImVec4(0.0f, 0.55f, 0.75f, 0.08f * alpha));
    ImU32 lit   = ImGui::GetColorU32(ImVec4(0.32f, 0.93f, 1.0f, 0.95f * alpha));

    for (int s = 0; s < 7; ++s) {
        if (m & (1 << s))
            draw_lcd_segment(dl,
                ImVec2(S[s][0].x - g * 0.6f, S[s][0].y - g * 0.6f),
                ImVec2(S[s][1].x + g * 0.6f, S[s][1].y + g * 0.6f),
                rounding + 2.0f, glow);
        else
            draw_lcd_segment(dl, S[s][0], S[s][1], rounding, ghost);
    }
    for (int s = 0; s < 7; ++s)
        if (m & (1 << s))
            draw_lcd_segment(dl, S[s][0], S[s][1], rounding, lit);
}

static void draw_lcd_time(ImDrawList* dl, int hh, int mm, bool colon, float x, float y, float H, float alpha) {
    float W = H * 0.62f;
    float gap = W * 0.35f;
    float colonW = H * 0.18f;
    char t[4] = { (char)('0' + hh / 10), (char)('0' + hh % 10), (char)('0' + mm / 10), (char)('0' + mm % 10) };
    float cx = x;
    for (int i = 0; i < 4; ++i) {
        draw_lcd_digit(dl, t[i], cx, y, W, H, alpha);
        cx += W + gap;
        if (i == 1) {
            float dot = colonW;
            if (colon) {
                draw_lcd_segment(dl,
                    ImVec2(cx, y + H * 0.30f), ImVec2(cx + dot, y + H * 0.30f + dot), dot * 0.3f,
                    ImGui::GetColorU32(ImVec4(0.32f, 0.93f, 1.0f, 0.95f * alpha)));
                draw_lcd_segment(dl,
                    ImVec2(cx, y + H * 0.58f), ImVec2(cx + dot, y + H * 0.58f + dot), dot * 0.3f,
                    ImGui::GetColorU32(ImVec4(0.32f, 0.93f, 1.0f, 0.95f * alpha)));
            }
            cx += dot + gap;
        }
    }
}

static void push_style() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.92f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.02f, 0.07f, 0.09f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.06f, 0.2f, 0.24f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
}



static void pop_style() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

void render_ui(bool expanded, int width, int height) {
    float p = g_progress;
    float iw = NOTCH_WIDTH + (EXPANDED_WIDTH - NOTCH_WIDTH) * p;
    float ih = NOTCH_HEIGHT + (EXPANDED_HEIGHT - NOTCH_HEIGHT) * p;
    float ix = ((float)width - iw) * 0.5f;
    float iy = 0.0f;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
    ImGui::Begin("Notch", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
    );

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rbr = fminf(RAD_BR, ih * 0.25f);
    float rbl = fminf(RAD_BL, ih * 0.25f);
    draw_island_shape(dl, ix, iy, iw, ih, RAD_TL, RAD_TR, rbr, rbl, g_window_alpha);

    push_style();

    SYSTEMTIME st;
    GetLocalTime(&st);
    bool colon_on = (st.wSecond % 2) == 0;

    if (expanded) {
        ImGui::SetCursorPos(ImVec2(ix, iy));
        ImGui::BeginChild("island_content", ImVec2(iw, ih));

        float cw = 120.0f;
        float xw = 80.0f;
        float mw = iw - cw - xw;

        ImGui::BeginChild("clock", ImVec2(cw, 0));

        draw_lcd_time(dl, st.wHour, st.wMinute, colon_on, ix + 10.0f, iy + 7.0f, 26.0f, g_window_alpha);

        char buf[64];
        ImGui::SetCursorPos(ImVec2(10, 42));
        ImGui::PushFont(g_font_small);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.8f, 0.9f, 0.9f));
        sprintf(buf, "%s, %s %d", days[st.wDayOfWeek], months[st.wMonth - 1], st.wDay);
        ImGui::Text("%s", buf);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("music", ImVec2(mw, 0));
        ImGui::SetCursorPos(ImVec2(0, 24));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.8f, 0.9f, 1.0f));
        ImGui::Text("    No music playing");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("controls", ImVec2(xw, 0));
        ImGui::SetCursorPos(ImVec2(8, 20));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.9f, 1.0f, 0.35f));
        ImGui::Button("▶", ImVec2(36, 36));
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::EndChild(); // island_content

    } else {
        float H = ih * 0.85f;
        float W = H * 0.62f;
        float gap = W * 0.35f;
        float colonW = H * 0.18f;
        float total = 4.0f * W + 4.0f * gap + colonW;
        float x = ix + (iw - total) * 0.5f;
        float y = iy + (ih - H) * 0.5f;
        draw_lcd_time(dl, st.wHour, st.wMinute, colon_on, x, y, H, g_window_alpha);
    }

    pop_style();
    ImGui::End();
}