#pragma once
#include <string>

struct HttpResult {
    long status = 0;
    std::string body;
};

// HTTPS GET or POST. Returns false on network failure (out.status may still be
// set on HTTP error responses). timeout_sec bounds the whole request.
bool http_request(bool post, const std::string& url, const std::string& headers,
    const std::string& body, HttpResult& out, int timeout_sec = 8);
