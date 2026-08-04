#include "json.h"
#include <cctype>
#include <cstdlib>

namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;
    explicit Parser(const std::string& str) : s(str) {}

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    }

    bool parse(JsonValue& v) {
        ws();
        if (i >= s.size()) return false;
        char c = s[i];
        if (c == '{') return parse_object(v);
        if (c == '[') return parse_array(v);
        if (c == '"') return parse_string(v);
        if (c == 't' || c == 'f') return parse_bool(v);
        if (c == 'n') { v.type = JsonValue::Null; i += 4; return true; }
        return parse_number(v);
    }

    bool parse_string(JsonValue& v) {
        if (i >= s.size() || s[i] != '"') return false;
        i++;
        std::string out;
        while (i < s.size() && s[i] != '"') {
            unsigned char ch = (unsigned char)s[i];
            if (ch == '\\') {
                i++;
                if (i >= s.size()) return false;
                char e = s[i];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (i + 4 >= s.size()) return false;
                        unsigned cp = 0;
                        for (int k = 1; k <= 4; k++) {
                            char h = s[i + k];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                            else return false;
                        }
                        i += 4;
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: return false;
                }
            } else {
                out += (char)ch;
            }
            i++;
        }
        if (i >= s.size()) return false;
        i++;
        v.type = JsonValue::String;
        v.s = std::move(out);
        return true;
    }

    bool parse_bool(JsonValue& v) {
        if (s.compare(i, 4, "true") == 0) { v.type = JsonValue::Bool; v.b = true; i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { v.type = JsonValue::Bool; v.b = false; i += 5; return true; }
        return false;
    }

    bool parse_number(JsonValue& v) {
        size_t start = i;
        if (i < s.size() && s[i] == '-') i++;
        while (i < s.size() && isdigit((unsigned char)s[i])) i++;
        if (i < s.size() && s[i] == '.') { i++; while (i < s.size() && isdigit((unsigned char)s[i])) i++; }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            i++;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) i++;
            while (i < s.size() && isdigit((unsigned char)s[i])) i++;
        }
        if (start == i) return false;
        v.type = JsonValue::Number;
        v.n = atof(s.substr(start, i - start).c_str());
        return true;
    }

    bool parse_array(JsonValue& v) {
        i++;
        v.type = JsonValue::Array;
        ws();
        if (i < s.size() && s[i] == ']') { i++; return true; }
        for (;;) {
            JsonValue item;
            if (!parse(item)) return false;
            v.arr.push_back(std::move(item));
            ws();
            if (i >= s.size()) return false;
            if (s[i] == ',') { i++; ws(); continue; }
            if (s[i] == ']') { i++; return true; }
            return false;
        }
    }

    bool parse_object(JsonValue& v) {
        i++;
        v.type = JsonValue::Object;
        ws();
        if (i < s.size() && s[i] == '}') { i++; return true; }
        for (;;) {
            ws();
            JsonValue key;
            if (i >= s.size() || s[i] != '"' || !parse_string(key)) return false;
            ws();
            if (i >= s.size() || s[i] != ':') return false;
            i++;
            JsonValue val;
            if (!parse(val)) return false;
            v.obj.emplace_back(key.s, std::move(val));
            ws();
            if (i >= s.size()) return false;
            if (s[i] == ',') { i++; continue; }
            if (s[i] == '}') { i++; return true; }
            return false;
        }
    }
};

} // namespace

bool json_parse(const std::string& text, JsonValue& out) {
    Parser p(text);
    return p.parse(out);
}
