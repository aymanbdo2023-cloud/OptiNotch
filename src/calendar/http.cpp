#include "http.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <vector>

static std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

bool http_request(bool post, const std::string& url, const std::string& headers,
    const std::string& body, HttpResult& out, int timeout_sec) {
    out.status = 0;
    out.body.clear();

    std::wstring wurl = widen(url);
    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t scheme[32], host[512], path[4096], extra[512];
    uc.lpszScheme = scheme; uc.dwSchemeLength = 32;
    uc.lpszHostName = host; uc.dwHostNameLength = 512;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 4096;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 512;
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) return false;

    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"OptiNotch/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    DWORD to = (DWORD)timeout_sec * 1000;
    WinHttpSetTimeouts(hSession, to, to, to, to);

    HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    std::wstring wpath = uc.lpszUrlPath ? uc.lpszUrlPath : L"/";
    if (uc.lpszExtraInfo && *uc.lpszExtraInfo) wpath += uc.lpszExtraInfo;

    HINTERNET hReq = WinHttpOpenRequest(hConnect, post ? L"POST" : L"GET", wpath.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    std::wstring wheaders = widen(headers);
    LPCWSTR hdr = headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wheaders.c_str();
    DWORD hdrlen = headers.empty() ? 0 : (DWORD)wheaders.size();
    LPVOID bod = body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data();
    DWORD bodlen = (DWORD)body.size();

    BOOL ok = WinHttpSendRequest(hReq, hdr, hdrlen, bod, bodlen, bodlen, 0);
    if (ok) ok = WinHttpReceiveResponse(hReq, nullptr);
    if (!ok) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0, len = sizeof(statusCode);
    if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &len, WINHTTP_NO_HEADER_INDEX))
        out.status = (long)statusCode;

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        std::vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(hReq, buf.data(), avail, &read) || read == 0) break;
        out.body.append(buf.data(), read);
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}
