#include "settings.h"
#include "../calendar/json.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <mutex>

namespace {

std::mutex g_mutex;
AppSettings g_settings;

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string appdata_dir() {
    char buf[MAX_PATH] = {};
    GetEnvironmentVariableA("APPDATA", buf, MAX_PATH);
    std::string dir = buf;
    if (dir.empty()) dir = ".";
    dir += "\\OptiNotch";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

std::string settings_path() {
    return appdata_dir() + "\\settings.json";
}

// Where the app should be launched from for auto-start. In a packaged build
// this is the standalone exe; in dev it is python + runner.py.
std::wstring autostart_command() {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring p = exe;
    size_t slash = p.find_last_of(L"\\/");
    std::wstring base = (slash == std::wstring::npos) ? p : p.substr(slash + 1);
    if (base == L"python.exe" || base == L"pythonw.exe") {
        wchar_t cwd[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, cwd);
        return p + L" \"" + std::wstring(cwd) + L"\\runner.py\"";
    }
    return p;
}

} // namespace

void settings_load() {
    std::lock_guard<std::mutex> lk(g_mutex);
    AppSettings s;

    JsonValue j;
    std::string txt = read_file(settings_path());
    if (!txt.empty() && json_parse(txt, j)) {
        s.monitor_index = (int)j.get_num("monitor", -1);
        s.x_offset = (int)j.get_num("x_offset", 0);
        s.hide_hotkey = j.get_bool("hide_hotkey", true);
        s.start_with_windows = j.get_bool("start_with_windows", false);
        s.accent_r = (int)j.get_num("accent_r", 92);
        s.accent_g = (int)j.get_num("accent_g", 147);
        s.accent_b = (int)j.get_num("accent_b", 255);
        s.opacity_normal = (float)j.get_num("opacity_normal", 240.0);
        s.opacity_hover = (float)j.get_num("opacity_hover", 180.0);
        s.media_enabled = j.get_bool("media_enabled", true);
        s.calendar_enabled = j.get_bool("calendar_enabled", true);
    }

    s.accent_r = s.accent_r < 0 ? 0 : (s.accent_r > 255 ? 255 : s.accent_r);
    s.accent_g = s.accent_g < 0 ? 0 : (s.accent_g > 255 ? 255 : s.accent_g);
    s.accent_b = s.accent_b < 0 ? 0 : (s.accent_b > 255 ? 255 : s.accent_b);
    if (s.opacity_normal < 0.0f) s.opacity_normal = 240.0f;
    if (s.opacity_hover < 0.0f) s.opacity_hover = 180.0f;

    g_settings = s;
}

void settings_save() {
    const AppSettings s = settings_get();
    std::ofstream f(settings_path());
    if (!f) return;
    f << "{\n";
    f << "  \"monitor\": " << s.monitor_index << ",\n";
    f << "  \"x_offset\": " << s.x_offset << ",\n";
    f << "  \"hide_hotkey\": " << (s.hide_hotkey ? "true" : "false") << ",\n";
    f << "  \"start_with_windows\": " << (s.start_with_windows ? "true" : "false") << ",\n";
    f << "  \"accent_r\": " << s.accent_r << ",\n";
    f << "  \"accent_g\": " << s.accent_g << ",\n";
    f << "  \"accent_b\": " << s.accent_b << ",\n";
    f << "  \"opacity_normal\": " << s.opacity_normal << ",\n";
    f << "  \"opacity_hover\": " << s.opacity_hover << ",\n";
    f << "  \"media_enabled\": " << (s.media_enabled ? "true" : "false") << ",\n";
    f << "  \"calendar_enabled\": " << (s.calendar_enabled ? "true" : "false") << "\n";
    f << "}\n";
}

AppSettings settings_get() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_settings;
}

void settings_set(const AppSettings& s) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_settings = s;
}

void settings_apply_autostart() {
    const AppSettings s = settings_get();
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;

    if (s.start_with_windows) {
        std::wstring cmd = autostart_command();
        RegSetValueExW(key, L"OptiNotch", 0, REG_SZ,
            (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, L"OptiNotch");
    }
    RegCloseKey(key);
}
