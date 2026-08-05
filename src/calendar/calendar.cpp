#include "calendar.h"
#include "json.h"
#include "http.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <fstream>
#include <sstream>
#include <algorithm>

std::mutex g_cal_mutex;
CalendarState g_cal;

static volatile bool g_cal_stop = false;
static HANDLE g_cal_thread = nullptr;

// ---- date helpers ----

static bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int cal_days_in_month(int y, int m) {
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 1 && is_leap(y)) return 29;
    return d[m];
}

int cal_day_of_week(int y, int m, int d) {
    int mm = m + 1;
    if (mm < 3) { mm += 12; y--; }
    int k = y % 100, j = y / 100;
    int h = (d + 13 * (mm + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7; // 0=Sat
    return (h + 6) % 7; // 0=Sun .. 6=Sat
}

static void add_days(int& y, int& m, int& d, int n) {
    d += n;
    while (d > cal_days_in_month(y, m)) { d -= cal_days_in_month(y, m); m++; if (m > 11) { m = 0; y++; } }
    while (d < 1) { m--; if (m < 0) { m = 11; y--; } d += cal_days_in_month(y, m); }
}

// ---- small string helpers ----

static std::string url_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            out += (char)(hex_val(s[i + 1]) * 16 + hex_val(s[i + 2]));
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

static std::string fmt_hm(int hh, int mm) {
    int h12 = hh % 12; if (h12 == 0) h12 = 12;
    char b[16];
    sprintf(b, "%d:%02d %s", h12, mm, hh < 12 ? "AM" : "PM");
    return b;
}

static std::string fmt_date(const std::string& d) {
    static const char* mnames[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
    int y = atoi(d.substr(0, 4).c_str());
    int m = atoi(d.substr(5, 2).c_str());
    int dd = atoi(d.substr(8, 2).c_str());
    (void)y;
    char b[32];
    sprintf(b, "%s %d", mnames[(m - 1) % 12], dd);
    return b;
}

// ---- credentials / token storage ----

struct Creds {
    bool ok = false;
    std::string client_id, client_secret;
    std::string calendar_id = "primary";
    int port = 8080;
};

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Per-user location of the OAuth client JSON (the setup wizard writes here).
static std::string creds_path() {
    char buf[MAX_PATH] = {};
    GetEnvironmentVariableA("APPDATA", buf, MAX_PATH);
    std::string dir = buf;
    if (dir.empty()) dir = ".";
    dir += "\\OptiNotch";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir + "\\gcal_credentials.json";
}

static Creds creds_from_text(const std::string& txt) {
    Creds c;
    JsonValue j;
    if (txt.empty() || !json_parse(txt, j)) return c;
    // Accept both the flat OptiNotch format and Google's raw "installed"/"web"
    // desktop-client JSON (nested wrapper) so a downloaded file works as-is.
    const JsonValue* base = &j;
    const JsonValue* inst = j.get("installed");
    const JsonValue* web = j.get("web");
    if (inst && inst->type == JsonValue::Object) base = inst;
    else if (web && web->type == JsonValue::Object) base = web;
    c.client_id = base->get_str("client_id");
    c.client_secret = base->get_str("client_secret");
    c.calendar_id = j.get_str("calendar_id", "primary");
    c.port = (int)j.get_num("redirect_port", 8080);
    if (c.client_id.empty() || c.client_secret.empty()) return c;
    if (c.port <= 0 || c.port > 65535) c.port = 8080;
    c.ok = true;
    return c;
}

// Directory of the running executable (for credentials bundled next to the
// packaged OptiNotch.exe).
static std::string exe_dir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p = buf;
    size_t slash = p.find_last_of(L"\\/");
    std::wstring dir = (slash == std::wstring::npos) ? L"." : p.substr(0, slash);
    char mb[MAX_PATH] = {};
    WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, mb, MAX_PATH, nullptr, nullptr);
    return mb;
}

static Creds load_credentials() {
    // Prefer the per-user file written by the setup wizard, then the app's own
    // directory (bundled credentials shipped with the release), then the
    // repo-root file so the dev flow keeps working.
    Creds c = creds_from_text(read_file(creds_path()));
    if (!c.ok) c = creds_from_text(read_file(exe_dir() + "\\gcal_credentials.json"));
    if (!c.ok) c = creds_from_text(read_file("gcal_credentials.json"));
    return c;
}

static std::string token_path() {
    char buf[MAX_PATH] = {};
    GetEnvironmentVariableA("APPDATA", buf, MAX_PATH);
    std::string dir = buf;
    if (dir.empty()) dir = ".";
    dir += "\\OptiNotch";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir + "\\gcal_token.json";
}

struct Token {
    bool ok = false;
    std::string access_token, refresh_token;
    double expires_at = 0.0;
};

static Token load_token() {
    Token t;
    std::string txt = read_file(token_path());
    JsonValue j;
    if (txt.empty() || !json_parse(txt, j)) return t;
    t.access_token = j.get_str("access_token");
    t.refresh_token = j.get_str("refresh_token");
    t.expires_at = j.get_num("expires_at", 0.0);
    t.ok = !t.refresh_token.empty();
    return t;
}

static void save_token(const Token& t) {
    std::ofstream f(token_path());
    if (!f) return;
    f << "{\n";
    f << "  \"access_token\": \"" << t.access_token << "\",\n";
    f << "  \"refresh_token\": \"" << t.refresh_token << "\",\n";
    f << "  \"expires_at\": " << t.expires_at << "\n}\n";
}

// ---- OAuth 2.0 ----

// Loopback redirect: wait (up to ~90s) for the browser to hit
// http://127.0.0.1:port/?code=... and extract the code.
static bool receive_oauth_code(int port, std::string& code, volatile bool* stop) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) { WSACleanup(); return false; }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)port);
    if (bind(ls, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(ls, 1) != 0) {
        closesocket(ls);
        WSACleanup();
        return false;
    }

    bool got = false;
    for (int i = 0; i < 450; i++) {
        if (stop && *stop) break;
        fd_set rf; FD_ZERO(&rf); FD_SET(ls, &rf);
        timeval tv = {}; tv.tv_usec = 200000;
        int r = select(0, &rf, nullptr, nullptr, &tv);
        if (r > 0) {
            SOCKET c = accept(ls, nullptr, nullptr);
            if (c != INVALID_SOCKET) {
                char req[8192] = {};
                recv(c, req, (int)sizeof(req) - 1, 0);
                const char* q = strstr(req, "code=");
                if (q) {
                    q += 5;
                    const char* sp = strchr(q, ' ');
                    const char* am = strchr(q, '&');
                    const char* end = am ? (am < sp ? am : sp) : sp;
                    size_t n = end ? (size_t)(end - q) : strlen(q);
                    code.assign(q, n);
                    got = !code.empty();
                }
                const char* body = "<html><body><h3>OptiNotch</h3><p>You can close this window now.</p></body></html>";
                char resp[512];
                sprintf(resp, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                    (int)strlen(body), body);
                send(c, resp, (int)strlen(resp), 0);
                closesocket(c);
                if (got) break;
            }
        }
    }
    closesocket(ls);
    WSACleanup();
    return got;
}

static double now_sec() { return (double)GetTickCount64() / 1000.0; }

static bool token_from_response(const std::string& body, Token& tok, bool with_refresh) {
    JsonValue j;
    if (!json_parse(body, j)) return false;
    tok.access_token = j.get_str("access_token");
    if (tok.access_token.empty()) return false;
    if (with_refresh) {
        std::string rt = j.get_str("refresh_token");
        if (!rt.empty()) tok.refresh_token = rt;
    }
    double expires_in = j.get_num("expires_in", 3600.0);
    tok.expires_at = now_sec() + expires_in - 120.0;
    return true;
}

static bool run_auth_flow(const Creds& creds, Token& tok, volatile bool* stop) {
    std::string redir = "http://127.0.0.1:" + std::to_string(creds.port) + "/";
    std::string authurl = "https://accounts.google.com/o/oauth2/v2/auth?client_id=" + url_encode(creds.client_id)
        + "&redirect_uri=" + url_encode(redir)
        + "&response_type=code&scope=" + url_encode("https://www.googleapis.com/auth/calendar.readonly")
        + "&access_type=offline&prompt=consent";
    ShellExecuteA(nullptr, "open", authurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    std::string code;
    if (!receive_oauth_code(creds.port, code, stop)) return false;
    if (code.empty()) return false;
    code = url_decode(code);

    std::string body = "grant_type=authorization_code&code=" + url_encode(code)
        + "&client_id=" + url_encode(creds.client_id)
        + "&client_secret=" + url_encode(creds.client_secret)
        + "&redirect_uri=" + url_encode(redir);
    HttpResult r;
    if (!http_request(true, "https://oauth2.googleapis.com/token",
        "Content-Type: application/x-www-form-urlencoded", body, r))
        return false;
    return token_from_response(r.body, tok, true);
}

static bool refresh_access(const Creds& creds, Token& tok) {
    std::string body = "grant_type=refresh_token&client_id=" + url_encode(creds.client_id)
        + "&client_secret=" + url_encode(creds.client_secret)
        + "&refresh_token=" + url_encode(tok.refresh_token);
    HttpResult r;
    if (!http_request(true, "https://oauth2.googleapis.com/token",
        "Content-Type: application/x-www-form-urlencoded", body, r))
        return false;
    return token_from_response(r.body, tok, false);
}

// ---- events ----

static bool fetch_events(const Creds& creds, const Token& tok, int year, int month,
    std::vector<CalEvent>& out) {
    int y0 = year, m0 = month, d0 = 1;
    add_days(y0, m0, d0, -3);
    int y1 = year, m1 = month, d1 = cal_days_in_month(year, month);
    add_days(y1, m1, d1, 3);
    char tmin[32], tmax[32];
    sprintf(tmin, "%04d-%02d-%02dT00:00:00Z", y0, m0 + 1, d0);
    sprintf(tmax, "%04d-%02d-%02dT23:59:59Z", y1, m1 + 1, d1);

    char url[2048];
    sprintf(url, "https://www.googleapis.com/calendar/v3/calendars/%s/events?singleEvents=true&orderBy=startTime&maxResults=50&timeMin=%s&timeMax=%s",
        url_encode(creds.calendar_id).c_str(), tmin, tmax);
    std::string hdr = "Authorization: Bearer " + tok.access_token;
    HttpResult r;
    if (!http_request(false, url, hdr, "", r)) return false;
    if (r.status == 401) return false; // caller refreshes token and retries

    JsonValue j;
    if (!json_parse(r.body, j)) return false;
    const JsonValue* items = j.get("items");
    if (!items) return false;

    char mon[8];
    sprintf(mon, "%04d-%02d", year, month + 1);

    std::vector<CalEvent> evs;
    for (const auto& it : items->arr) {
        const JsonValue* start = it.get("start");
        if (!start) continue;
        std::string date;
        std::string time;
        bool all_day = false;
        if (start->get("date")) {
            all_day = true;
            date = start->get_str("date");
        } else if (start->get("dateTime")) {
            std::string dt = start->get_str("dateTime");
            if (dt.size() >= 10) date = dt.substr(0, 10);
            if (dt.size() >= 16) time = dt.substr(11, 5);
        } else {
            continue;
        }
        if (date.size() < 10 || date.compare(0, 7, mon) != 0) continue;

        CalEvent e;
        e.summary = it.get_str("summary", "(no title)");
        e.all_day = all_day;
        e.day = atoi(date.substr(8, 2).c_str());
        int hh = 0, mm = 0;
        if (!time.empty()) {
            hh = atoi(time.substr(0, 2).c_str());
            mm = atoi(time.substr(3, 2).c_str());
        }
        e.sort_key = e.day * 1440 + hh * 60 + mm;
        e.when = fmt_date(date);
        e.when += all_day ? " · All-day" : (" · " + fmt_hm(hh, mm));
        evs.push_back(std::move(e));
    }

    std::stable_sort(evs.begin(), evs.end(),
        [](const CalEvent& a, const CalEvent& b) { return a.sort_key < b.sort_key; });
    out = std::move(evs);
    return true;
}

// ---- background thread ----

DWORD WINAPI cal_thread_main(void*) {
    Creds creds = load_credentials();
    {
        std::lock_guard<std::mutex> lk(g_cal_mutex);
        g_cal.has_credentials = creds.ok;
        if (!creds.ok) g_cal.status = "Set up gcal_credentials.json";
    }

    Token tok;
    bool authed = false;
    int cur_y = -1, cur_m = -1;
    double last_fetch = 0.0;

    // Last wall-clock month/year seen by the loop. Used to roll the displayed
    // month over when the date changes (midnight, sleep/resume) while the user
    // is viewing "today".
    SYSTEMTIME st0;
    GetLocalTime(&st0);
    int known_y = st0.wYear, known_m = st0.wMonth - 1;

    while (!g_cal_stop) {
        // Keep "today" (and the displayed month when it is the current one) in
        // sync with the wall clock. The date is only computed once at init, so
        // it would otherwise go stale after the machine sleeps or a day/month
        // boundary rolls past.
        SYSTEMTIME st;
        GetLocalTime(&st);
        {
            std::lock_guard<std::mutex> lk(g_cal_mutex);
            g_cal.today_day = st.wDay;
            bool viewing_known = (g_cal.year == known_y && g_cal.month == known_m);
            bool month_changed = (st.wYear != known_y || (st.wMonth - 1) != known_m);
            if (month_changed && viewing_known) {
                g_cal.year = st.wYear;
                g_cal.month = st.wMonth - 1;
                g_cal.selected_day = 0;
                g_cal.refresh_requested = true;
            }
        }
        known_y = st.wYear;
        known_m = st.wMonth - 1;

        bool refresh = false, pending = false, signout = false;
        int year, month;
        {
            std::lock_guard<std::mutex> lk(g_cal_mutex);
            refresh = g_cal.refresh_requested;
            g_cal.refresh_requested = false;
            pending = g_cal.auth_pending;
            signout = g_cal.sign_out_requested;
            g_cal.sign_out_requested = false;
            year = g_cal.year;
            month = g_cal.month;
        }

        if (signout) {
            // The user signed out: drop the in-memory token and any cached
            // events. Next loop iteration reloads the (now missing) token file
            // and waits for the user to press Connect again.
            authed = false;
            tok = Token{};
            {
                std::lock_guard<std::mutex> lk(g_cal_mutex);
                g_cal.authed = false;
                g_cal.events.clear();
                g_cal.busy = false;
                g_cal.status = "Signed out \xE2\x80\x94 press Connect";
            }
            continue;
        }

        if (!creds.ok) {
            // The user may have just imported credentials via the wizard;
            // reload so we pick them up without a restart.
            Creds fresh = load_credentials();
            if (fresh.ok) {
                creds = fresh;
                std::lock_guard<std::mutex> lk(g_cal_mutex);
                g_cal.has_credentials = true;
                g_cal.refresh_requested = true;
                continue;
            }
            Sleep(500);
            continue;
        }

        if (!authed) {
            bool attempted = false;
            {
                std::lock_guard<std::mutex> lk(g_cal_mutex);
                attempted = g_cal.auth_attempted;
            }
            tok = load_token();
            if (tok.ok && !tok.refresh_token.empty()) {
                authed = true;
                std::lock_guard<std::mutex> lk(g_cal_mutex);
                g_cal.authed = true;
                g_cal.status = "";
            } else if (pending || !attempted) {
                {
                    std::lock_guard<std::mutex> lk(g_cal_mutex);
                    g_cal.auth_attempted = true;
                    g_cal.auth_pending = false;
                    g_cal.busy = true;
                    g_cal.status = "Waiting for Google sign-in…";
                }
                bool stop = false;
                if (run_auth_flow(creds, tok, &stop)) {
                    save_token(tok);
                    authed = true;
                    std::lock_guard<std::mutex> lk(g_cal_mutex);
                    g_cal.authed = true;
                    g_cal.busy = false;
                    g_cal.status = "";
                    g_cal.refresh_requested = true;
                } else {
                    std::lock_guard<std::mutex> lk(g_cal_mutex);
                    g_cal.busy = false;
                    g_cal.authed = false;
                    g_cal.status = "Sign-in failed — press Connect";
                }
            } else {
                Sleep(1000);
                continue;
            }
        }

        if (authed) {
            double now = now_sec();
            if (tok.expires_at <= now + 30.0) {
                if (!refresh_access(creds, tok)) {
                    std::lock_guard<std::mutex> lk(g_cal_mutex);
                    g_cal.authed = false;
                    g_cal.status = "Session expired — press Connect";
                    authed = false;
                    Sleep(1000);
                    continue;
                }
                save_token(tok);
            }

            if (refresh || year != cur_y || month != cur_m || now - last_fetch > 600.0) {
                cur_y = year;
                cur_m = month;
                {
                    std::lock_guard<std::mutex> lk(g_cal_mutex);
                    g_cal.busy = true;
                    g_cal.status = "";
                }
                std::vector<CalEvent> evs;
                bool ok = fetch_events(creds, tok, year, month, evs);
                if (!ok && refresh_access(creds, tok)) {
                    save_token(tok);
                    ok = fetch_events(creds, tok, year, month, evs);
                }
                std::lock_guard<std::mutex> lk(g_cal_mutex);
                if (ok) {
                    g_cal.events = std::move(evs);
                    g_cal.status = "";
                    last_fetch = now;
                } else {
                    g_cal.status = "Calendar sync failed";
                }
                g_cal.busy = false;
            }
        }

        Sleep(300);
    }
    return 0;
}

// ---- public API ----

void cal_init() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    {
        std::lock_guard<std::mutex> lk(g_cal_mutex);
        g_cal.year = st.wYear;
        g_cal.month = st.wMonth - 1;
        g_cal.today_day = st.wDay;
        g_cal.selected_day = 0;
        g_cal.authed = false;
        g_cal.auth_pending = false;
        g_cal.auth_attempted = false;
        g_cal.refresh_requested = true;
        g_cal.busy = false;
    }
    g_cal_stop = false;
    g_cal_thread = CreateThread(nullptr, 0, cal_thread_main, nullptr, 0, nullptr);
}

void cal_shutdown() {
    g_cal_stop = true;
    if (g_cal_thread) {
        WaitForSingleObject(g_cal_thread, 8000);
        CloseHandle(g_cal_thread);
        g_cal_thread = nullptr;
    }
}

void cal_goto_month(int delta) {
    std::lock_guard<std::mutex> lk(g_cal_mutex);
    g_cal.month += delta;
    if (g_cal.month < 0) { g_cal.month = 11; g_cal.year--; }
    if (g_cal.month > 11) { g_cal.month = 0; g_cal.year++; }
    g_cal.refresh_requested = true;
}

void cal_select_day(int day) {
    std::lock_guard<std::mutex> lk(g_cal_mutex);
    g_cal.selected_day = day;
}

void cal_goto_today() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::lock_guard<std::mutex> lk(g_cal_mutex);
    g_cal.year = st.wYear;
    g_cal.month = st.wMonth - 1;
    g_cal.selected_day = 0;
    g_cal.refresh_requested = true;
}

void cal_request_refresh() {
    std::lock_guard<std::mutex> lk(g_cal_mutex);
    g_cal.refresh_requested = true;
}

void cal_start_auth() {
    std::lock_guard<std::mutex> lk(g_cal_mutex);
    g_cal.auth_pending = true;
    g_cal.auth_attempted = false;
    g_cal.refresh_requested = true;
}

void cal_sign_out() {
    // Remove the token so the next sign-in starts fresh (account picker shows).
    DeleteFileA(token_path().c_str());
    std::lock_guard<std::mutex> lk(g_cal_mutex);
    g_cal.sign_out_requested = true;
    g_cal.authed = false;
    g_cal.auth_pending = false;
    // Don't auto-reconnect after sign-out; the user presses Connect explicitly.
    g_cal.auth_attempted = true;
    g_cal.events.clear();
    g_cal.refresh_requested = true;
}

bool cal_import_credentials_file(const std::wstring& path, std::string& err) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) {
        err = "Could not open that file.";
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();

    Creds c = creds_from_text(ss.str());
    if (!c.ok) {
        err = "No client_id/client_secret found. Download the OAuth Desktop "
              "client JSON from Google Cloud.";
        return false;
    }

    std::ofstream out(creds_path(), std::ios::binary);
    if (!out) {
        err = "Could not write to %APPDATA%\\OptiNotch.";
        return false;
    }
    out << ss.str();

    {
        std::lock_guard<std::mutex> lk(g_cal_mutex);
        g_cal.has_credentials = true;
        g_cal.refresh_requested = true;
    }
    return true;
}

void cal_open_cloud_setup() {
    ShellExecuteA(nullptr, "open",
        "https://console.cloud.google.com/apis/credentials",
        nullptr, nullptr, SW_SHOWNORMAL);
}
