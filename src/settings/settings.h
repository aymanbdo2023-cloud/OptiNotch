#pragma once
#include <string>

// User-editable options, persisted to %APPDATA%\OptiNotch\settings.json.
// The UI reads a snapshot via settings_get(); mutations go through
// settings_set() and are flushed to disk with settings_save().
struct AppSettings {
    int monitor_index = -1;      // -1 = primary monitor, 0..N-1 = nth enumerated monitor
    int x_offset = 0;            // logical px to shift the notch from the monitor's center
    bool hide_hotkey = true;     // Win+Alt slides the notch away / back
    bool start_with_windows = false;
    int accent_r = 92, accent_g = 147, accent_b = 255;  // 0..255 accent used across the UI
    float opacity_normal = 240.0f;  // notch fill alpha (0..255) at rest
    float opacity_hover = 180.0f;   // fill alpha when hovered while holding Ctrl
    bool media_enabled = true;      // start the media (SMTC) session poller
    bool calendar_enabled = true;   // start the Google Calendar poller
};

void settings_load();                  // read settings.json (defaults if missing/unparseable)
void settings_save();                  // write settings.json to disk
AppSettings settings_get();            // snapshot of the current settings
void settings_set(const AppSettings& s);  // replace the in-memory settings
void settings_apply_autostart();       // sync the HKCU\...\Run value with start_with_windows
