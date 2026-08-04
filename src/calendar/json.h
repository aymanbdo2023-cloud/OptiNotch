#pragma once
#include <string>
#include <vector>
#include <utility>

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    bool is_null() const { return type == Null; }
    const JsonValue* get(const std::string& key) const {
        for (const auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    const JsonValue* at(size_t i) const { return i < arr.size() ? &arr[i] : nullptr; }
    size_t size() const { return type == Array ? arr.size() : (type == Object ? obj.size() : 0); }
    bool get_bool(const std::string& k, bool dflt = false) const {
        const JsonValue* v = get(k); return (v && v->type == Bool) ? v->b : dflt;
    }
    std::string get_str(const std::string& k, const std::string& dflt = "") const {
        const JsonValue* v = get(k); return (v && v->type == String) ? v->s : dflt;
    }
    double get_num(const std::string& k, double dflt = 0.0) const {
        const JsonValue* v = get(k); return (v && v->type == Number) ? v->n : dflt;
    }
};

bool json_parse(const std::string& text, JsonValue& out);
