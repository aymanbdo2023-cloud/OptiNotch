#pragma once
#include <string>
#include <vector>
#include <mutex>

struct CalEvent {
    std::string summary;
    std::string when;   // display metadata: "Aug 14 · All-day" / "Aug 14 · 9:00 AM"
    int day = 0;
    int sort_key = 0;   // day*1440 + minutes
    bool all_day = false;
};

struct CalendarState {
    bool has_credentials = false;
    bool authed = false;
    bool auth_pending = false;
    bool auth_attempted = false;
    bool sign_out_requested = false;   // cleared by the calendar thread
    bool refresh_requested = true;
    bool busy = false;
    int year = 0, month = 0;   // displayed (month 0-11)
    int today_day = 0;
    int selected_day = 0;
    std::string status;        // short UI status line
    std::vector<CalEvent> events;
};

extern std::mutex g_cal_mutex;
extern CalendarState g_cal;

int cal_days_in_month(int y, int m);
int cal_day_of_week(int y, int m, int d);

void cal_init();
void cal_shutdown();
void cal_goto_month(int delta);
void cal_select_day(int day);
void cal_goto_today();
void cal_request_refresh();
void cal_start_auth();

// Clears the stored token and un-authenticates so the user can sign in with a
// different Google account (the notch then shows "Connect Google Calendar").
void cal_sign_out();

// First-run setup helpers (used by the in-notch wizard).
bool cal_import_credentials_file(const std::wstring& path, std::string& err);
void cal_open_cloud_setup();
