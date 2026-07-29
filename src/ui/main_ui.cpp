#include "main_ui.h"
#include <imgui.h>
#include <cstdio>
#include <Windows.h>


static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


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

void render_ui(bool expanded, int width, int height) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
    ImGui::Begin("Notch", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
    );

    push_style();

    SYSTEMTIME st;
    GetLocalTime(&st);

    char buf[64];

    if (expanded) {
        float cw = 120.0f;
        float xw = 80.0f;
        float mw = (float)width - cw - xw;

        ImGui::BeginChild("clock", ImVec2(cw, 0));

        ImGui::SetCursorPos(ImVec2(10, 5));
        ImGui::PushFont(g_font_clock);
        sprintf(buf, "%02d:%02d", st.wHour, st.wMinute);
        ImGui::Text("%s", buf);
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(10, 42));
        ImGui::PushFont(g_font_small);
        sprintf(buf, "%s, %s %d", days[st.wDayOfWeek], months[st.wMonth - 1], st.wDay);
        ImGui::Text("%s", buf);
        ImGui::PopFont();

        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("music", ImVec2(mw, 0));
        ImGui::SetCursorPos(ImVec2(0, 24));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.75f, 1.0f));
        ImGui::Text("    No music playing");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("controls", ImVec2(xw, 0));
        ImGui::SetCursorPos(ImVec2(8, 20));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.2f));
        ImGui::Button("▶", ImVec2(36, 36));
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::EndChild();

    } else {
        ImGui::SetCursorPos(ImVec2(4, 1));
        ImGui::PushFont(g_font_collapsed);
        sprintf(buf, "%02d:%02d", st.wHour, st.wMinute);
        ImGui::Text("%s", buf);
        ImGui::PopFont();
    }

    pop_style();
    ImGui::End();
}