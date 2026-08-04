#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <mutex>

struct MediaState {
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    bool has_media = false;
    bool is_playing = false;
    int status = 0;
    double position_sec = 0.0;
    double duration_sec = 0.0;
    double snapshot_time_sec = 0.0;
    bool can_prev = false;
    bool can_play = false;
    bool can_pause = false;
    bool can_next = false;
    std::vector<unsigned char> art;
    unsigned int art_rev = 0;
};

extern std::mutex g_media_mutex;
extern MediaState g_media;

void media_init();
void media_update();
void media_shutdown();
void media_toggle_play();
void media_skip_previous();
void media_skip_next();
bool media_has_active_session();
