#include "media.h"
#include <roapi.h>
#include <winstring.h>
#include <algorithm>
#include <stdio.h>

namespace media_abi {
enum DispatcherQueueThreadApartmentType { DQTAT_COM_NONE = 0, DQTAT_COM_ASTA = 1, DQTAT_COM_STA = 2 };
enum DispatcherQueueThreadType { DQTYPE_THREAD_DEDICATED = 1, DQTYPE_THREAD_CURRENT = 2 };
struct DispatcherQueueOptions {
    DWORD dwSize;
    DispatcherQueueThreadType threadType;
    DispatcherQueueThreadApartmentType apartmentType;
};
EXTERN_C HRESULT WINAPI CreateDispatcherQueueController(DispatcherQueueOptions options, void** dispatcherQueueController);
} // namespace media_abi

static bool media_debug_enabled() {
    static bool on = [] {
        char buf[2] = {};
        return GetEnvironmentVariableA("OPTINOTCH_MEDIA_DEBUG", buf, 2) > 0;
    }();
    return on;
}

#define DBG(fmt, ...) do { \
    if (media_debug_enabled()) { \
        FILE* _f = fopen("media_dbg.log", "a"); \
        if (_f) { fprintf(_f, fmt "\n", ##__VA_ARGS__); fclose(_f); } \
    } \
} while (0)

namespace media_abi {

struct IInspectable : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG* iidCount, IID** iids) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* className) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* trustLevel) = 0;
};

struct IAsyncInfo : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Id(unsigned int* id) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Status(int* status) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT* errorCode) = 0;
    virtual HRESULT STDMETHODCALLTYPE Cancel() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};

struct IAsyncOperationCompletedHandler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(void* operation, int status) = 0;
};

struct IAsyncOperation : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(void* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(void** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(void** results) = 0;
};

struct IAsyncOperationWithProgress : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Progress(void* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Progress(void** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Completed(void* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(void** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(void** results) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionManagerStatics : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE RequestAsync(IAsyncOperation** operation) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionManager : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSession(void** session) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSessions(void** sessions) = 0;
};

struct IGlobalSystemMediaTransportControlsSession : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE SourceAppUserModelId(HSTRING* id) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryGetMediaPropertiesAsync(IAsyncOperation** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTimelineProperties(void** timeline) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPlaybackInfo(void** playbackInfo) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryPlayAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryPauseAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryStopAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryRecordAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryFastForwardAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryRewindAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TrySkipNextAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TrySkipPreviousAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryChangeChannelUpAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryChangeChannelDownAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryTogglePlayPauseAsync(void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryChangeAutoRepeatModeAsync(int mode, void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryChangePlaybackRateAsync(double rate, void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryChangeShuffleActiveAsync(int active, void** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryChangePlaybackPositionAsync(__int64 position, void** operation) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionMediaProperties : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Title(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE Subtitle(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE AlbumArtist(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE Artist(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE AlbumTitle(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE TrackNumber(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE Genres(void** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE AlbumTrackCount(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE PlaybackType(void** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE Thumbnail(void** value) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionPlaybackInfo : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Controls(void** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE PlaybackStatus(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE PlaybackType(void** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE AutoRepeatMode(void** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE PlaybackRate(void** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsShuffleActive(void** value) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionPlaybackControls : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE IsPlayEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsPauseEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsStopEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRecordEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsFastForwardEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRewindEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsNextEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsPreviousEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsChannelUpEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsChannelDownEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsPlayPauseToggleEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsShuffleEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRepeatEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsPlaybackRateEnabled(int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsPlaybackPositionEnabled(int* value) = 0;
};

struct IGlobalSystemMediaTransportControlsSessionTimelineProperties : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE StartTime(__int64* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE EndTime(__int64* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE MinSeekTime(__int64* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE MaxSeekTime(__int64* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE Position(__int64* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE LastUpdatedTime(__int64* value) = 0;
};

struct IRandomAccessStreamReference : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE OpenReadAsync(IAsyncOperation** operation) = 0;
};

struct IBuffer;

struct IInputStream : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE ReadAsync(IBuffer* buffer, unsigned int count, int options, IAsyncOperationWithProgress** operation) = 0;
};

struct IBuffer : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Capacity(unsigned int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Length(unsigned int* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Length(unsigned int value) = 0;
};

struct IBufferFactory : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Create(unsigned int capacity, IBuffer** value) = 0;
};

struct IBufferByteAccess : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Buffer(BYTE** value) = 0;
};

} // namespace media_abi

using media_abi::IGlobalSystemMediaTransportControlsSession;
using media_abi::IGlobalSystemMediaTransportControlsSessionManager;
using media_abi::IGlobalSystemMediaTransportControlsSessionManagerStatics;
using media_abi::IGlobalSystemMediaTransportControlsSessionMediaProperties;
using media_abi::IGlobalSystemMediaTransportControlsSessionPlaybackInfo;
using media_abi::IGlobalSystemMediaTransportControlsSessionPlaybackControls;
using media_abi::IGlobalSystemMediaTransportControlsSessionTimelineProperties;
using media_abi::IRandomAccessStreamReference;
using media_abi::IInputStream;
using media_abi::IBuffer;
using media_abi::IBufferFactory;
using media_abi::IBufferByteAccess;

static const IID IID_MgrStatics = { 0x2050c4ee, 0x11a0, 0x57de, { 0xae, 0xd7, 0xc9, 0x7c, 0x70, 0x33, 0x82, 0x45 } };
static const IID IID_IAsyncInfo = { 0x00000036, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const IID IID_IInputStream = { 0x905a0fe2, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };
static const IID IID_IBufferByteAccess = { 0x905a0fef, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };
static const IID IID_IBufferFactory = { 0x71af914d, 0xc10f, 0x484b, { 0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27 } };

static const wchar_t* kBufferClassName = L"Windows.Storage.Streams.Buffer";

static MediaState g_snapshot;
static volatile bool g_thread_stop = false;
static HANDLE g_thread = nullptr;
static IGlobalSystemMediaTransportControlsSession* g_session_ptr = nullptr;

std::mutex g_media_mutex;
MediaState g_media;

namespace {

typedef HRESULT(STDMETHODCALLTYPE* Slot1)(void*, void*);

HRESULT wait_for_completion(IUnknown* op, int get_results_slot, void** result, DWORD timeout_ms = 8000) {
    media_abi::IAsyncInfo* info = nullptr;
    HRESULT hr = op->QueryInterface(IID_IAsyncInfo, (void**)&info);
    if (FAILED(hr) || !info) {
        DBG("    wait QI IAsyncInfo hr=0x%08X", (unsigned)hr);
        return hr ? hr : E_NOINTERFACE;
    }

    void** vtbl = *(void***)op;
    int last = -1;
    DWORD start = GetTickCount();
    for (;;) {
        void* res = nullptr;
        hr = ((Slot1)vtbl[get_results_slot])(op, &res);
        if (SUCCEEDED(hr) && res) {
            *result = res;
            info->Release();
            return S_OK;
        }

        int status = 0;
        info->get_Status(&status);
        if (status == 4) {
            HRESULT e = 0;
            info->get_ErrorCode(&e);
            info->Release();
            return e ? e : E_FAIL;
        }
        if (status == 3) {
            info->Release();
            return E_ABORT;
        }
        if (GetTickCount() - start >= timeout_ms) {
            info->Release();
            DBG("    wait timeout status=%d last hr=0x%08X", status, (unsigned)hr);
            return (HRESULT)0x800705B4L;
        }
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }
}

void read_string(HRESULT hr, HSTRING h, std::wstring& out) {
    if (SUCCEEDED(hr) && h) {
        unsigned int len = 0;
        const wchar_t* p = WindowsGetStringRawBuffer(h, &len);
        out.assign(p, len);
        WindowsDeleteString(h);
    } else {
        if (h) WindowsDeleteString(h);
        out.clear();
    }
}

void read_stream(IInputStream* in, std::vector<unsigned char>& art) {
    HSTRING cls = nullptr;
    HRESULT hr = WindowsCreateString(kBufferClassName, (UINT32)wcslen(kBufferClassName), &cls);
    if (FAILED(hr)) return;

    IBufferFactory* factory = nullptr;
    hr = RoGetActivationFactory(cls, IID_IBufferFactory, (void**)&factory);
    WindowsDeleteString(cls);
    if (FAILED(hr) || !factory) return;

    const unsigned int CHUNK = 65536;
    std::vector<unsigned char> data;
    IBuffer* buf = nullptr;
    if (SUCCEEDED(factory->Create(CHUNK, &buf))) {
        for (;;) {
            media_abi::IAsyncOperationWithProgress* op = nullptr;
            if (FAILED(in->ReadAsync(buf, CHUNK, 0, &op))) break;
            void* res = nullptr;
            if (FAILED(wait_for_completion(op, 10, &res))) { op->Release(); break; }
            op->Release();

            IBuffer* readbuf = (IBuffer*)res;
            unsigned int len = 0;
            readbuf->get_Length(&len);
            IBufferByteAccess* acc = nullptr;
            if (SUCCEEDED(readbuf->QueryInterface(IID_IBufferByteAccess, (void**)&acc))) {
                BYTE* p = nullptr;
                acc->Buffer(&p);
                if (p && len) data.insert(data.end(), p, p + len);
                acc->Release();
            }
            readbuf->Release();
            if (len < CHUNK) break;
        }
        buf->Release();
    }
    factory->Release();

    if (data.empty()) {
        DBG("    art stream: 0 bytes read");
        return;
    }
    if (art.size() != data.size() || !std::equal(art.begin(), art.end(), data.begin()))
        art = std::move(data);
    DBG("    art stream: %u bytes", (unsigned)art.size());
}

void read_art(IGlobalSystemMediaTransportControlsSessionMediaProperties* props, MediaState& s) {
    void* thumb = nullptr;
    if (FAILED(props->Thumbnail(&thumb)) || !thumb) {
        DBG("    Thumbnail: null");
        if (thumb) ((IUnknown*)thumb)->Release();
        return;
    }
    IRandomAccessStreamReference* ref = (IRandomAccessStreamReference*)thumb;

    std::vector<unsigned char> data;
    media_abi::IAsyncOperation* op = nullptr;
    if (SUCCEEDED(ref->OpenReadAsync(&op))) {
        void* stream = nullptr;
        if (SUCCEEDED(wait_for_completion(op, 8, &stream))) {
            IInputStream* in = nullptr;
            if (stream && SUCCEEDED(((IUnknown*)stream)->QueryInterface(IID_IInputStream, (void**)&in))) {
                read_stream(in, data);
                in->Release();
            }
            if (stream) ((IUnknown*)stream)->Release();
        }
        op->Release();
    }
    ref->Release();

    if (data.empty()) {
        if (!s.art.empty()) {
            s.art.clear();
            s.art_rev++;
        }
        return;
    }
    if (s.art.size() != data.size() || !std::equal(s.art.begin(), s.art.end(), data.begin())) {
        s.art = std::move(data);
        s.art_rev++;
    }
}

void update_snapshot(IGlobalSystemMediaTransportControlsSession* session) {
    MediaState s;
    s.snapshot_time_sec = (double)GetTickCount64() / 1000.0;

    if (session) {
        media_abi::IAsyncOperation* op = nullptr;
        HRESULT hr = session->TryGetMediaPropertiesAsync(&op);
        if (SUCCEEDED(hr) && op) {
            void* props = nullptr;
            if (SUCCEEDED(wait_for_completion(op, 8, &props)) && props) {
                IGlobalSystemMediaTransportControlsSessionMediaProperties* p =
                    (IGlobalSystemMediaTransportControlsSessionMediaProperties*)props;
                HSTRING h = nullptr;
                read_string(p->Title(&h), h, s.title);
                read_string(p->Artist(&h), h, s.artist);
                read_string(p->AlbumTitle(&h), h, s.album);
                DBG("  title='%ls' artist='%ls'", s.title.c_str(), s.artist.c_str());
                read_art(p, s);
                p->Release();
            }
            op->Release();
        }

        void* info = nullptr;
        if (SUCCEEDED(session->GetPlaybackInfo(&info)) && info) {
            IGlobalSystemMediaTransportControlsSessionPlaybackInfo* pi =
                (IGlobalSystemMediaTransportControlsSessionPlaybackInfo*)info;
            pi->PlaybackStatus(&s.status);
            s.is_playing = (s.status == 4);
            void* ctrl = nullptr;
            if (SUCCEEDED(pi->Controls(&ctrl)) && ctrl) {
                IGlobalSystemMediaTransportControlsSessionPlaybackControls* c =
                    (IGlobalSystemMediaTransportControlsSessionPlaybackControls*)ctrl;
                int v = 0;
                c->IsPreviousEnabled(&v); s.can_prev = !!v;
                c->IsPlayEnabled(&v); s.can_play = !!v;
                c->IsPauseEnabled(&v); s.can_pause = !!v;
                c->IsNextEnabled(&v); s.can_next = !!v;
                c->Release();
            }
            pi->Release();
        }

        void* tl = nullptr;
        if (SUCCEEDED(session->GetTimelineProperties(&tl)) && tl) {
            IGlobalSystemMediaTransportControlsSessionTimelineProperties* t =
                (IGlobalSystemMediaTransportControlsSessionTimelineProperties*)tl;
            __int64 pos = 0, end = 0;
            t->Position(&pos);
            t->EndTime(&end);
            s.position_sec = (double)pos / 10000000.0;
            s.duration_sec = (double)end / 10000000.0;
            t->Release();
        }

        s.has_media = (s.status == 4 || s.status == 5) && !s.title.empty();
    }

    {
        std::lock_guard<std::mutex> lk(g_media_mutex);
        g_snapshot = s;
    }
}

DWORD WINAPI media_thread_main(void*) {
    RoInitialize(RO_INIT_SINGLETHREADED);

    media_abi::DispatcherQueueOptions dqo{};
    dqo.dwSize = sizeof(dqo);
    dqo.threadType = media_abi::DQTYPE_THREAD_CURRENT;
    dqo.apartmentType = media_abi::DQTAT_COM_STA;
    void* dqController = nullptr;
    HRESULT dqhr = media_abi::CreateDispatcherQueueController(dqo, &dqController);
    DBG("CreateDispatcherQueueController hr=0x%08X ctrl=%p", (unsigned)dqhr, dqController);

    HSTRING cls = nullptr;
    HRESULT hr = WindowsCreateString(L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager",
                                     (UINT32)wcslen(L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager"),
                                     &cls);
    DBG("WindowsCreateString hr=0x%08X", (unsigned)hr);
    IGlobalSystemMediaTransportControlsSessionManagerStatics* statics = nullptr;
    if (SUCCEEDED(hr)) {
        hr = RoGetActivationFactory(cls, IID_MgrStatics, (void**)&statics);
        DBG("RoGetActivationFactory hr=0x%08X statics=%p", (unsigned)hr, (void*)statics);
        WindowsDeleteString(cls);
    }

    if (SUCCEEDED(hr) && statics) {
        media_abi::IAsyncOperation* op = nullptr;
        hr = statics->RequestAsync(&op);
        DBG("RequestAsync hr=0x%08X op=%p", (unsigned)hr, (void*)op);
        if (SUCCEEDED(hr) && op) {
            void* mgr = nullptr;
            hr = wait_for_completion(op, 8, &mgr, 30000);
            DBG("RequestAsync wait hr=0x%08X mgr=%p", (unsigned)hr, mgr);
            if (SUCCEEDED(hr) && mgr) {
                IGlobalSystemMediaTransportControlsSessionManager* manager =
                    (IGlobalSystemMediaTransportControlsSessionManager*)mgr;
                    while (!g_thread_stop) {
                        IGlobalSystemMediaTransportControlsSession* session = nullptr;
                        HRESULT hr2 = manager->GetCurrentSession((void**)&session);
                        if (SUCCEEDED(hr2)) {
                            update_snapshot(session);
                        } else {
                            DBG("poll GetCurrentSession hr=0x%08X", (unsigned)hr2);
                        }
                        {
                            std::lock_guard<std::mutex> lk(g_media_mutex);
                            if (g_session_ptr) g_session_ptr->Release();
                            g_session_ptr = session;
                        }
                        for (int i = 0; i < 5 && !g_thread_stop; i++) Sleep(100);
                    }
                    manager->Release();
                }
            op->Release();
        }
        statics->Release();
    } else {
        DBG("activation failed, hr=0x%08X", (unsigned)hr);
    }

    {
        std::lock_guard<std::mutex> lk(g_media_mutex);
        g_snapshot = MediaState();
        if (g_session_ptr) { g_session_ptr->Release(); g_session_ptr = nullptr; }
    }

    if (dqController) ((IUnknown*)dqController)->Release();
    RoUninitialize();
    return 0;
}

} // namespace

void media_init() {
    g_thread_stop = false;
    g_thread = CreateThread(nullptr, 0, media_thread_main, nullptr, 0, nullptr);
}

void media_update() {
    std::lock_guard<std::mutex> lk(g_media_mutex);
    g_media.title = g_snapshot.title;
    g_media.artist = g_snapshot.artist;
    g_media.album = g_snapshot.album;
    g_media.has_media = g_snapshot.has_media;
    g_media.is_playing = g_snapshot.is_playing;
    g_media.status = g_snapshot.status;
    g_media.position_sec = g_snapshot.position_sec;
    g_media.duration_sec = g_snapshot.duration_sec;
    g_media.snapshot_time_sec = g_snapshot.snapshot_time_sec;
    g_media.can_prev = g_snapshot.can_prev;
    g_media.can_play = g_snapshot.can_play;
    g_media.can_pause = g_snapshot.can_pause;
    g_media.can_next = g_snapshot.can_next;
    if (g_media.art_rev != g_snapshot.art_rev) {
        g_media.art = g_snapshot.art;
        g_media.art_rev = g_snapshot.art_rev;
    }
}

void media_shutdown() {
    g_thread_stop = true;
    if (g_thread) {
        WaitForSingleObject(g_thread, 3000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}

static void fire_and_forget(HRESULT (IGlobalSystemMediaTransportControlsSession::* method)(void**)) {
    IGlobalSystemMediaTransportControlsSession* session = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_media_mutex);
        session = g_session_ptr;
        if (session) session->AddRef();
    }
    if (session) {
        void* op = nullptr;
        (session->*method)(&op);
        if (op) ((IUnknown*)op)->Release();
        session->Release();
    }
}

void media_toggle_play() { fire_and_forget(&IGlobalSystemMediaTransportControlsSession::TryTogglePlayPauseAsync); }
void media_skip_previous() { fire_and_forget(&IGlobalSystemMediaTransportControlsSession::TrySkipPreviousAsync); }
void media_skip_next() { fire_and_forget(&IGlobalSystemMediaTransportControlsSession::TrySkipNextAsync); }

bool media_has_active_session() {
    std::lock_guard<std::mutex> lk(g_media_mutex);
    return g_snapshot.status == 4 || g_snapshot.status == 5;
}
